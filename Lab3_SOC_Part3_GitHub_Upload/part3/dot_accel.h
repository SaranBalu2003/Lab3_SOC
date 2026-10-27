#ifndef LAB3_DOT_ACCEL_H
#define LAB3_DOT_ACCEL_H
#ifndef SC_INCLUDE_DYNAMIC_PROCESSES
#define SC_INCLUDE_DYNAMIC_PROCESSES
#endif
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <array>
#include "dot_math.h"

class dot_accel : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<dot_accel> socket;
    sc_core::sc_in<bool> reset;
    sc_core::sc_out<bool> irq;
    SC_HAS_PROCESS(dot_accel);
    explicit dot_accel(sc_core::sc_module_name name);
private:
    std::array<unsigned char, DOT_BUFFER_BYTES> x_, y_;
    uint32_t status_, elements_, result_, error_, irq_enable_, cycles_;
    uint64_t epoch_;
    sc_core::sc_event start_event_, irq_event_;
    void b_transport(tlm::tlm_generic_payload &, sc_core::sc_time &);
    void worker();
    void reset_state();
    void update_irq();
    void signal_irq();
};
#endif
