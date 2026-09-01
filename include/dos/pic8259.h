/**
 * @file pic8259.h
 * @brief Intel 8259A Programmable Interrupt Controller (master, single mode).
 *
 * Ported from the proven, chip-accurate Py86 emulator (pic8259.py) to C++23 so
 * that tdx's DOS machine can deliver *hardware* interrupts (timer/keyboard) to
 * the guest through the IVT, instead of only bumping the BIOS tick dword.
 *
 * The PIC has no CPU reference here (unlike Py86); delivery is polled by the
 * machine at instruction boundaries via pending_ack()/ack_vector().
 */
#ifndef DOS_PIC8259_H
#define DOS_PIC8259_H

#include <cstdint>

class pic8259
{
public:
    pic8259() = default;

    /* Power-on / ICW1 reset defaults. */
    void reset()
    {
        m_irr = 0x00;
        m_isr = 0x00;
        m_imr = 0xFF;
        m_vector_base = 0x08;
        m_icw_step = 0;
        m_initialized = false;
        m_auto_eoi = false;
        m_special_mask = false;
        m_read_isr = true;
        m_trigger_mode = 0;
        m_icw4_needed = false;
        m_single_mode = false;
        m_irq_lines = 0x00;
    }

    /* Port 0x20 command write: ICW1 / OCW2 / OCW3. */
    void write_command(uint8_t value)
    {
        if (value & 0x10u)
        { /* ICW1 */
            m_icw_step = 1;
            m_initialized = false;
            m_imr = 0x00;
            m_isr = 0x00;
            m_irr = 0x00;
            m_irq_lines = 0x00;
            m_trigger_mode = (value & 0x08u) ? 1 : 0;
            m_icw4_needed = (value & 0x01u) != 0;
            m_single_mode = (value & 0x02u) != 0;
            return;
        }
        if (value & 0x20u)
        { /* OCW2: EOI */
            if (value & 0x40u)
            { /* specific EOI */
                const unsigned bit = value & 0x07u;
                m_isr = (uint8_t)(m_isr & ~(1u << bit));
            }
            else
            {
                eoi();
            }
            return;
        }
        if (value & 0x08u)
        { /* OCW3 */
            if (value & 0x02u)
            {
                m_read_isr = (value & 0x01u) != 0;
            }
            if (value & 0x40u)
            {
                m_special_mask = (value & 0x20u) != 0;
            }
        }
    }

    /* Port 0x21 data write: ICW2-4 during init, else OCW1 (mask). */
    void write_data(uint8_t value)
    {
        if (!m_initialized)
        {
            if (m_icw_step == 1)
            {
                m_vector_base = (uint8_t)(value & 0xF8u);
                m_icw_step = 2;
            }
            else if (m_icw_step == 2)
            {
                if (m_icw4_needed)
                {
                    if (m_single_mode)
                    {
                        m_auto_eoi = (value & 0x02u) != 0;
                        m_initialized = true;
                        m_icw_step = 0;
                    }
                    else
                    {
                        m_icw_step = 3; /* cascade: this was ICW3 */
                    }
                }
                else
                {
                    m_initialized = true;
                    m_icw_step = 0;
                }
            }
            else if (m_icw_step == 3)
            {
                m_auto_eoi = (value & 0x02u) != 0;
                m_initialized = true;
                m_icw_step = 0;
            }
            return;
        }

        const uint8_t old = m_imr;
        m_imr = value;
        (void)old; /* unmasking may expose a pending IRQ; polled each boundary */
    }

    /* Port 0x20 read: IRR or ISR per OCW3. */
    uint8_t read_command() const
    {
        return m_read_isr ? m_isr : m_irr;
    }

    /* Port 0x21 read: always the IMR. */
    uint8_t read_data() const
    {
        return m_imr;
    }

    /* Device asserts INTx (rising-edge latch in edge mode). */
    void assert_irq(unsigned irq)
    {
        if (irq > 7u)
        {
            return;
        }
        const uint8_t bit = (uint8_t)(1u << irq);
        if (m_trigger_mode == 0)
        { /* edge: latch only on low->high */
            if ((m_irq_lines & bit) == 0u)
            {
                m_irr = (uint8_t)(m_irr | bit);
            }
            m_irq_lines = (uint8_t)(m_irq_lines | bit);
        }
        else
        { /* level: IRR follows pin */
            m_irr = (uint8_t)(m_irr | bit);
            m_irq_lines = (uint8_t)(m_irq_lines | bit);
        }
    }

    /* Device releases INTx. */
    void deassert_irq(unsigned irq)
    {
        if (irq > 7u)
        {
            return;
        }
        const uint8_t bit = (uint8_t)(1u << irq);
        m_irq_lines = (uint8_t)(m_irq_lines & ~bit);
        if (m_trigger_mode == 1)
        {
            m_irr = (uint8_t)(m_irr & ~bit);
        }
    }

    /* Is there an unmasked, pending interrupt the CPU should acknowledge?
     * Py86 signals via cpu.request_intr(); here the machine polls this at each
     * instruction boundary (equivalent, and keeps the PIC CPU-agnostic). */
    bool pending() const
    {
        return (m_irr & (uint8_t)(~m_imr)) != 0u;
    }

    /* CPU INTA cycle: return vector (vector_base+irq) or 0xFF if none. */
    uint8_t ack_vector()
    {
        if (!m_initialized)
        {
            return 0xFF;
        }
        const uint8_t pending = (uint8_t)(m_irr & (uint8_t)(~m_imr));
        if (pending == 0u)
        {
            return 0xFF;
        }
        for (unsigned irq = 0u; irq < 8u; irq++)
        {
            if (pending & (1u << irq))
            {
                m_irr = (uint8_t)(m_irr & ~(1u << irq));
                m_isr = (uint8_t)(m_isr | (1u << irq));
                if (m_auto_eoi)
                {
                    m_isr = (uint8_t)(m_isr & ~(1u << irq));
                }
                return (uint8_t)(m_vector_base + irq);
            }
        }
        return 0xFF;
    }

    /* Like ack_vector but does not consume the request / set ISR. */
    uint8_t peek_vector() const
    {
        if (!m_initialized)
        {
            return 0xFF;
        }
        const uint8_t pending = (uint8_t)(m_irr & (uint8_t)(~m_imr));
        if (pending == 0u)
        {
            return 0xFF;
        }
        return (uint8_t)(m_vector_base + (unsigned)__builtin_ctz((unsigned)pending));
    }

    /* tdx skips BIOS POST, so apply the state DOS programs expect after boot:
     * initialized, vectors at 08h+, timer (IRQ0) and keyboard (IRQ1) unmasked. */
    void pc_boot_state()
    {
        reset();
        m_initialized = true;
        m_vector_base = 0x08;
        m_imr = 0xFC; /* IRQ0 + IRQ1 enabled, rest masked (typical PC state) */
    }

    /* Non-specific EOI: clear highest-priority (lowest-numbered) ISR bit. */
    void eoi()
    {
        if (m_isr != 0u)
        {
            const unsigned irq = (unsigned)__builtin_ctz((unsigned)m_isr);
            m_isr = (uint8_t)(m_isr & ~(1u << irq));
        }
    }

    uint8_t irr() const { return m_irr; }
    uint8_t isr() const { return m_isr; }
    uint8_t imr() const { return m_imr; }
    bool initialized() const { return m_initialized; }
    uint8_t vector_base() const { return m_vector_base; }

private:
    uint8_t m_irr = 0x00;
    uint8_t m_isr = 0x00;
    uint8_t m_imr = 0xFF;
    uint8_t m_vector_base = 0x08;
    unsigned m_icw_step = 0;
    bool m_initialized = false;
    bool m_auto_eoi = false;
    bool m_special_mask = false;
    bool m_read_isr = true;
    int m_trigger_mode = 0;
    bool m_icw4_needed = false;
    bool m_single_mode = false;
    uint8_t m_irq_lines = 0x00;
};

#endif /* DOS_PIC8259_H */
