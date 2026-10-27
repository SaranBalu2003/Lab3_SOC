#include "dot_accel.h"
#include <cstring>
using namespace sc_core;

dot_accel::dot_accel(sc_module_name name)
    : sc_module(name), socket("socket"), reset("reset"), irq("irq"),
      status_(0), elements_(0), result_(0), error_(0), irq_enable_(0), cycles_(0), epoch_(0) {
    x_.fill(0); y_.fill(0);
    socket.register_b_transport(this, &dot_accel::b_transport);
    SC_THREAD(worker);
    SC_METHOD(reset_state);
    sensitive << reset;
    SC_METHOD(update_irq);
    sensitive << irq_event_;
}
void dot_accel::signal_irq() { irq_event_.notify(SC_ZERO_TIME); }
void dot_accel::update_irq() { irq.write(irq_enable_ && (status_ & DOT_STATUS_DONE)); }
void dot_accel::reset_state() {
    if (!reset.read()) return;
    ++epoch_;
    start_event_.cancel();
    status_ = elements_ = result_ = error_ = irq_enable_ = cycles_ = 0;
    x_.fill(0); y_.fill(0);
    signal_irq();
}
void dot_accel::worker() {
    for (;;) {
        wait(start_event_);
        if (!(status_ & DOT_STATUS_BUSY) || reset.read()) continue;
        const uint64_t generation = epoch_;
        uint32_t pending_result = 0;
        const dot_math::Error pending_error = dot_math::compute(x_.data(), y_.data(), elements_, pending_result);
        const sc_time duration(double(cycles_) * DOT_CLOCK_NS, SC_NS);
        wait(duration, reset.posedge_event());
        if (reset.read() || epoch_ != generation) continue;
        result_ = pending_error == dot_math::OK ? pending_result : 0;
        error_ = pending_error;
        status_ = DOT_STATUS_DONE | (pending_error == dot_math::OK ? 0u : DOT_STATUS_ERROR);
        signal_irq();
    }
}
void dot_accel::b_transport(tlm::tlm_generic_payload &t, sc_time &delay) {
    t.set_dmi_allowed(false);
    const bool rd = t.is_read(), wr = t.is_write();
    const uint64_t addr = t.get_address();
    const unsigned len = t.get_data_length();
    unsigned char *data = t.get_data_ptr();
    if (!rd && !wr) { t.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE); return; }
    if (t.get_byte_enable_ptr()) { t.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE); return; }
    if (!data || len == 0 || t.get_streaming_width() < len) {
        t.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE); return;
    }
    // Blocking timing: consume incoming annotation and the assumed bus-service
    // time before performing side effects. New work starts at this timestamp.
    if (delay != SC_ZERO_TIME) { wait(delay); delay = SC_ZERO_TIME; }
    wait(sc_time(DOT_BUS_NS, SC_NS));
    if (reset.read()) { t.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE); return; }
    unsigned char *buffer = NULL;
    uint64_t offset = 0;
    if (addr >= DOT_X_OFFSET && addr < DOT_X_OFFSET + DOT_BUFFER_BYTES) {
        buffer = x_.data(); offset = addr - DOT_X_OFFSET;
    } else if (addr >= DOT_Y_OFFSET && addr < DOT_Y_OFFSET + DOT_BUFFER_BYTES) {
        buffer = y_.data(); offset = addr - DOT_Y_OFFSET;
    }
    if (buffer) {
        if (len > DOT_BUFFER_BYTES - offset) { t.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE); return; }
        if (wr && (status_ & DOT_STATUS_BUSY)) { t.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE); return; }
        if (rd) std::memcpy(data, buffer + offset, len);
        else std::memcpy(buffer + offset, data, len);
        t.set_response_status(tlm::TLM_OK_RESPONSE); return;
    }
    if (addr > DOT_REG_CAPACITY) { t.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE); return; }
    if ((addr & 3u) || len != 4) { t.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE); return; }
    uint32_t v = rd ? 0 : dot_load_le32(data);
    if (rd) {
        switch (addr) {
        case DOT_REG_CONTROL: v = 0; break;
        case DOT_REG_STATUS: v = status_; break;
        case DOT_REG_N_ELEMENTS: v = elements_; break;
        case DOT_REG_RESULT: v = result_; break;
        case DOT_REG_ERROR: v = error_; break;
        case DOT_REG_IRQ_ENABLE: v = irq_enable_; break;
        case DOT_REG_ID: v = DOT_ID; break;
        case DOT_REG_CYCLES: v = cycles_; break;
        case DOT_REG_CAPACITY: v = DOT_MAX_ELEMENTS; break;
        default: t.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE); return;
        }
        dot_store_le32(data, v);
    } else {
        switch (addr) {
        case DOT_REG_CONTROL:
            if (status_ & DOT_STATUS_BUSY) { t.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE); return; }
            if (v == DOT_CONTROL_ACK) { status_ = error_ = 0; signal_irq(); }
            else if (v == DOT_CONTROL_START && !(status_ & DOT_STATUS_DONE)) {
                result_ = error_ = 0;
                if (elements_ > DOT_MAX_ELEMENTS || elements_ % DOT_BLOCK_ELEMENTS) {
                    cycles_ = 0; error_ = dot_math::BAD_LENGTH;
                    status_ = DOT_STATUS_DONE | DOT_STATUS_ERROR; signal_irq();
                } else {
                    cycles_ = dot_math::cycles(elements_);
                    status_ = DOT_STATUS_BUSY;
                    start_event_.notify(SC_ZERO_TIME);
                }
            } else { t.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE); return; }
            break;
        case DOT_REG_N_ELEMENTS:
            if (status_ & DOT_STATUS_BUSY) { t.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE); return; }
            elements_ = v; break;
        case DOT_REG_IRQ_ENABLE:
            if (v > 1) { t.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE); return; }
            irq_enable_ = v; signal_irq(); break;
        default: t.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE); return;
        }
    }
    t.set_response_status(tlm::TLM_OK_RESPONSE);
}
