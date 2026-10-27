#include "dot_accel.h"
#include "golden_cases.h"
#include <tlm_utils/simple_initiator_socket.h>
#include <array>
#include <cstring>
#include <iostream>
using namespace sc_core;

class Tester : public sc_module {
public:
    tlm_utils::simple_initiator_socket<Tester> socket;
    sc_out<bool> reset;
    sc_in<bool> irq;
    unsigned failures;
    SC_HAS_PROCESS(Tester);
    Tester(sc_module_name n) : sc_module(n), socket("socket"), reset("reset"), irq("irq"), failures(0) { SC_THREAD(run); }
private:
    void check(bool ok, const char *what) { if (!ok) { ++failures; std::cerr << "FAIL: " << what << " at " << sc_time_stamp() << "\n"; } }
    tlm::tlm_response_status tx(bool write, uint64_t addr, unsigned char *data, unsigned len,
                              bool enable_bytes = false, unsigned width = 0, unsigned incoming_ns = 0) {
        tlm::tlm_generic_payload p;
        unsigned char byte_enable = 255;
        p.set_command(write ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND);
        p.set_address(addr); p.set_data_ptr(data); p.set_data_length(len);
        p.set_streaming_width(width ? width : len);
        if (enable_bytes) { p.set_byte_enable_ptr(&byte_enable); p.set_byte_enable_length(1); }
        p.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sc_time d(incoming_ns, SC_NS);
        socket->b_transport(p, d);
        check(!p.is_dmi_allowed(), "DMI must remain disabled");
        check(d == SC_ZERO_TIME, "Incoming annotation is consumed");
        return p.get_response_status();
    }
    void wr(uint32_t addr, uint32_t value, tlm::tlm_response_status expected = tlm::TLM_OK_RESPONSE) {
        unsigned char p[4]; dot_store_le32(p, value);
        check(tx(true, addr, p, 4) == expected, "Register write response");
    }
    uint32_t rd(uint32_t addr) {
        unsigned char p[4] = {};
        check(tx(false, addr, p, 4) == tlm::TLM_OK_RESPONSE, "Register read response");
        return dot_load_le32(p);
    }
    void settle() { wait(SC_ZERO_TIME); wait(SC_ZERO_TIME); wait(SC_ZERO_TIME); }
    void pulse_reset() { reset.write(true); wait(sc_time(1, SC_NS)); reset.write(false); settle(); }
    void write_inputs(const fixtures::Case &c) {
        fixtures::RawBlock rx[DOT_MAX_BLOCKS], ry[DOT_MAX_BLOCKS];
        fixtures::fill(c, rx, ry);
        std::array<unsigned char, DOT_BUFFER_BYTES> x = {}, y = {};
        for (unsigned b = 0; b < DOT_MAX_BLOCKS; ++b) {
            unsigned char *xp = x.data()+b*DOT_SLOT_BYTES, *yp = y.data()+b*DOT_SLOT_BYTES;
            xp[0] = rx[b].d & 255; xp[1] = rx[b].d >> 8;
            yp[0] = ry[b].d & 255; yp[1] = ry[b].d >> 8;
            std::memcpy(xp+2, rx[b].qs, 32); std::memcpy(yp+2, ry[b].qs, 32);
        }
        check(tx(true, DOT_X_OFFSET, x.data(), x.size()) == tlm::TLM_OK_RESPONSE, "Write X buffer");
        check(tx(true, DOT_Y_OFFSET, y.data(), y.size()) == tlm::TLM_OK_RESPONSE, "Write Y buffer");
    }
    void complete(uint32_t elements) { wait(sc_time(double(dot_math::cycles(elements))*DOT_CLOCK_NS+1, SC_NS)); settle(); }
    void run() {
        pulse_reset();
        check(rd(DOT_REG_ID) == DOT_ID, "Peripheral ID");
        check(rd(DOT_REG_CAPACITY) == DOT_MAX_ELEMENTS, "Capacity");
        check(rd(DOT_REG_STATUS) == 0 && !irq.read(), "Reset state");
        unsigned char bytes[8] = {};
        check(tx(false, DOT_REG_ID, bytes, 4, true) == tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE, "Reject byte enables");
        check(tx(false, DOT_REG_ID, bytes, 8, false, 4) == tlm::TLM_BURST_ERROR_RESPONSE, "Reject streaming bursts");
        check(tx(false, 0x28, bytes, 4) == tlm::TLM_ADDRESS_ERROR_RESPONSE, "Reject unmapped registers");
        check(tx(false, 1, bytes, 4) == tlm::TLM_BURST_ERROR_RESPONSE, "Reject unaligned register reads");
        check(tx(true, DOT_X_OFFSET+DOT_BUFFER_BYTES-2, bytes, 4) == tlm::TLM_ADDRESS_ERROR_RESPONSE, "Reject buffer crossing");
        wr(DOT_REG_ID, 0, tlm::TLM_COMMAND_ERROR_RESPONSE);
        const sc_time before = sc_time_stamp();
        check(tx(false, DOT_REG_ID, bytes, 4, false, 0, 7) == tlm::TLM_OK_RESPONSE, "Annotated read");
        check(sc_time_stamp()-before == sc_time(7+DOT_BUS_NS, SC_NS), "Bus/incoming timing");
        wr(DOT_REG_IRQ_ENABLE, 1);
        wr(DOT_REG_N_ELEMENTS, 4097); wr(DOT_REG_CONTROL, DOT_CONTROL_START); settle();
        check(rd(DOT_REG_STATUS) == (DOT_STATUS_DONE|DOT_STATUS_ERROR), "Invalid length completes with error");
        check(rd(DOT_REG_ERROR) == dot_math::BAD_LENGTH && irq.read(), "Invalid length IRQ");
        wr(DOT_REG_CONTROL, DOT_CONTROL_ACK); settle(); check(!irq.read(), "ACK deasserts IRQ");

        const fixtures::Case timing = {"timing", 128, 1, 0, 0, 0};
        write_inputs(timing); wr(DOT_REG_N_ELEMENTS, 4096); wr(DOT_REG_CONTROL, DOT_CONTROL_START);
        const sc_time started = sc_time_stamp();
        settle();
        const sc_time duration(double(dot_math::cycles(4096))*DOT_CLOCK_NS, SC_NS);
        wait(duration - sc_time(DOT_BUS_NS+1, SC_NS));
        check(rd(DOT_REG_STATUS) == DOT_STATUS_BUSY, "Still busy one ns before modeled completion");
        check(!irq.read(), "No early completion IRQ");
        wait(sc_time(1, SC_NS)); settle();
        check(irq.read(), "IRQ asserts at modeled completion");
        check(sc_time_stamp()-started == duration, "Modeled full-vector delay");
        check(rd(DOT_REG_RESULT) == 0x45800000u, "128-block all-ones result is 4096");
        check(rd(DOT_REG_CYCLES) == 141, "Full-length timing register");
        wr(DOT_REG_IRQ_ENABLE, 0); settle(); check(!irq.read(), "Disable completion IRQ");
        wr(DOT_REG_IRQ_ENABLE, 1); settle(); check(irq.read(), "Enable pending completion IRQ");
        wr(DOT_REG_CONTROL, DOT_CONTROL_START, tlm::TLM_GENERIC_ERROR_RESPONSE);
        wr(DOT_REG_CONTROL, DOT_CONTROL_ACK); settle();

        write_inputs(timing); wr(DOT_REG_N_ELEMENTS, 4096); wr(DOT_REG_CONTROL, DOT_CONTROL_START);
        wr(DOT_REG_N_ELEMENTS, 32, tlm::TLM_GENERIC_ERROR_RESPONSE);
        wr(DOT_REG_CONTROL, DOT_CONTROL_START, tlm::TLM_GENERIC_ERROR_RESPONSE);
        wr(DOT_REG_CONTROL, DOT_CONTROL_ACK, tlm::TLM_GENERIC_ERROR_RESPONSE);
        check(tx(true, DOT_X_OFFSET, bytes, 4) == tlm::TLM_GENERIC_ERROR_RESPONSE, "Protect inputs while busy");
        pulse_reset(); wait(sc_time(1500, SC_NS));
        check(rd(DOT_REG_STATUS) == 0 && !irq.read(), "Reset cancels an outstanding computation");

        // Start a new job after reset; exercise a nonfinite active scale.
        bytes[0] = 0; bytes[1] = 0x7c; bytes[2] = bytes[3] = 1;
        check(tx(true, DOT_X_OFFSET, bytes, 4) == tlm::TLM_OK_RESPONSE, "Write Inf scale");
        wr(DOT_REG_N_ELEMENTS, 32); wr(DOT_REG_CONTROL, DOT_CONTROL_START); complete(32);
        check(rd(DOT_REG_ERROR) == dot_math::NONFINITE_SCALE, "Reject nonfinite scales");
        wr(DOT_REG_CONTROL, DOT_CONTROL_ACK);

        wr(DOT_REG_IRQ_ENABLE, 1);
        const unsigned count = sizeof golden_cases/sizeof golden_cases[0];
        for (unsigned i = 0; i < count; ++i) {
            const fixtures::Case &c = golden_cases[i];
            write_inputs(c); const uint32_t elements = c.blocks*32;
            wr(DOT_REG_N_ELEMENTS, elements); wr(DOT_REG_CONTROL, DOT_CONTROL_START);
            complete(elements);
            check(rd(DOT_REG_STATUS) == DOT_STATUS_DONE, c.name);
            check(rd(DOT_REG_RESULT) == c.expected_bits, c.name);
            check(irq.read(), "Completion IRQ");
            wr(DOT_REG_CONTROL, DOT_CONTROL_ACK); settle();
            check(!irq.read(), "ACK clears completion IRQ");
        }
        std::cout << (failures ? "SYSTEMC TESTS FAILED" : "SYSTEMC TESTS PASSED")
                  << ": " << count << " golden vectors plus protocol/timing/reset checks; failures=" << failures << "\n";
        sc_stop();
    }
};
int sc_main(int, char **) {
    sc_set_time_resolution(1, SC_PS);
    sc_signal<bool> reset_signal, irq_signal;
    dot_accel accel("accel"); Tester tester("tester");
    accel.reset(reset_signal); accel.irq(irq_signal);
    tester.reset(reset_signal); tester.irq(irq_signal); tester.socket.bind(accel.socket);
    sc_start();
    return tester.failures ? 1 : 0;
}
