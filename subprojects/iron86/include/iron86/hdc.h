/**
 * @file hdc.h
 * @brief IBM XT Fixed Disk Adapter (Xebec S1410-class SASI) at 320h–323h.
 *
 * Host map matches IBM 5160 Tech Ref + Fixed Disk ROS (not generic S1410A
 * linear LBA). DCB sector is 0-based (ROS `DEC CL`). IRQ 5, DMA channel 3.
 */
#ifndef IRON86_HDC_H
#define IRON86_HDC_H

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace iron86
{

class cpu;
struct dma8237;

/** @brief ST-412 / IBM Type-1 geometry (~10 MB). */
inline constexpr uint32_t k_hdc_cyl_xt = 306u;
inline constexpr uint32_t k_hdc_heads_xt = 4u;
inline constexpr uint32_t k_hdc_spt_xt = 17u;
inline constexpr uint32_t k_hdc_sector_size = 512u;

/** @brief ISA data port (DCB, parameters, sector data, completion, sense). */
inline constexpr uint16_t k_hdc_port_data = 0x320u;
/** @brief Status (R) / controller reset (W). */
inline constexpr uint16_t k_hdc_port_status = 0x321u;
/** @brief Select pulse (W) / configuration switches (R). */
inline constexpr uint16_t k_hdc_port_select = 0x322u;
/** @brief DMA enable (bit0) + IRQ enable (bit1) (W). */
inline constexpr uint16_t k_hdc_port_mask = 0x323u;
/** @brief ROS probe port; treated as a no-op / switch nibble. */
inline constexpr uint16_t k_hdc_port_probe = 0x213u;

inline constexpr uint8_t k_hdc_irq = 5u;
inline constexpr uint8_t k_hdc_dma_ch = 3u;

inline constexpr uint8_t k_hdc_stat_req = 0x01u;
inline constexpr uint8_t k_hdc_stat_io = 0x02u;
inline constexpr uint8_t k_hdc_stat_cd = 0x04u;
inline constexpr uint8_t k_hdc_stat_bsy = 0x08u;
inline constexpr uint8_t k_hdc_stat_drq = 0x10u;
inline constexpr uint8_t k_hdc_stat_irq = 0x20u;

/** @brief Physical map for IBM 5000059 option ROM. */
inline constexpr uint32_t k_hdc_ros_phys = 0xC8000u;
inline constexpr uint16_t k_hdc_ros_seg = 0xC800u;

/**
 * @brief Default IBM Xebec ROS dump (8 KiB, 55 AA, init at C800:0003).
 * @note Override with `--hdc-rom` or `IRON86_XEBEC_ROM`.
 */
inline constexpr const char *k_hdc_ros_default =
    "/mnt/TurboDebugger/Py86/ROM/IBM/XT/FixedDiskAdapter/IBM_XEBEC_5000059_1982.BIN";

/**
 * @brief Xebec S1410–compatible Fixed Disk Adapter (IBM XT personality).
 *
 * @thread_safety Not thread-safe. Bound to one @ref pc.
 */
class hdc_xebec
{
public:
    hdc_xebec();
    ~hdc_xebec();
    hdc_xebec(const hdc_xebec &) = delete;
    hdc_xebec &operator=(const hdc_xebec &) = delete;

    /**
     * @brief Attach a flat 512-byte-sector image and open it for update.
     *
     * Short files are zero-padded in RAM to @p cylinders × @p heads × @p spt
     * sectors. Writes are flushed to @p path (the file grows if needed).
     *
     * @param[in] path      Existing regular file (must not be NULL).
     * @param[in] cylinders Cylinder count (Type-1 default 306).
     * @param[in] heads     Head count (Type-1 default 4).
     * @param[in] spt       Sectors per track (Type-1 default 17).
     * @return false if @p path cannot be opened or geometry is zero.
     */
    bool attach(const char *path, uint32_t cylinders = k_hdc_cyl_xt,
                uint32_t heads = k_hdc_heads_xt, uint32_t spt = k_hdc_spt_xt);

    /** @brief Close the image file and return to idle (no IRQ). */
    void reset();

    /**
     * @brief Non-owning DMA/CPU pointers for channel-3 transfers.
     *
     * @param[in] c   Guest CPU (NULL disables DMA; PIO still works).
     * @param[in] dma 8237 state; channel 3 addr/count/page 82h.
     */
    void bind_bus(cpu *c, dma8237 *dma);

    /**
     * @brief Write ISA port 320h–323h (and 213h no-op).
     * @param[in] port I/O address.
     * @param[in] value Byte.
     */
    void write_port(uint16_t port, uint8_t value);

    /**
     * @brief Read ISA port 320h–323h (322h = DIP 0x0F).
     * @param[in] port I/O address.
     * @return Data, status, or 0xFF if the port is unused.
     */
    uint8_t read_port(uint16_t port);

    /**
     * @brief PIC IRQ5 line: latched completion/sense and 323h bit1 set.
     * @return true when the host adapter should assert IRQ5.
     */
    bool irq_line() const;

    /** @brief True after a successful @ref attach. */
    bool attached() const
    {
        return attached_;
    }

    uint32_t cylinders() const
    {
        return cylinders_;
    }
    uint32_t heads() const
    {
        return heads_;
    }
    uint32_t spt() const
    {
        return spt_;
    }
    uint32_t sector_count() const
    {
        return sector_count_;
    }

    /** @brief In-memory image (padded to geometry). */
    const std::vector<uint8_t> &image() const
    {
        return image_;
    }

    /** @brief Writable image bytes for mlock (NULL if empty). */
    uint8_t *image_data()
    {
        return image_.empty() ? nullptr : image_.data();
    }

    /** @brief Host path passed to @ref attach, or empty. */
    const std::string &path() const
    {
        return path_;
    }

private:
    enum class phase : uint8_t
    {
        idle,
        recv_cmd,
        recv_init,
        data_in,
        data_out,
        send_status,
        send_sense
    };

    void set_idle();
    void raise_irq();
    void clear_irq();
    void fail(uint8_t error_type, uint8_t error_code, uint8_t drive);
    void ok(uint8_t drive);
    void arm_completion();
    void write_data(uint8_t value);
    uint8_t read_data();
    void exec_dcb();
    bool parse_chs(uint8_t *drive, uint32_t *head, uint32_t *cyl, uint32_t *sector,
                   uint32_t *count);
    bool lba_ok(uint32_t cyl, uint32_t head, uint32_t sector, uint32_t count,
                uint32_t *lba) const;
    void start_read();
    void start_write();
    void start_verify();
    void format_range(uint32_t lba, uint32_t nsec);
    void finish_write();
    void dma_to_memory();
    void dma_from_memory();
    bool persist(uint32_t off, uint32_t n);
    void close_file();
    static uint8_t switch_nibble();

    cpu *cpu_;
    dma8237 *dma_;
    std::FILE *fp_;
    std::vector<uint8_t> image_;
    std::string path_;
    uint32_t cylinders_;
    uint32_t heads_;
    uint32_t spt_;
    uint32_t sector_count_;
    phase phase_;
    uint8_t status_;
    uint8_t dma_irq_;
    uint8_t dcb_[6];
    uint8_t dcb_n_;
    uint8_t completion_;
    uint8_t sense_[4];
    uint8_t sense_idx_;
    std::vector<uint8_t> buf_;
    uint32_t buf_pos_;
    uint16_t cur_cyl_[2];
    bool selected_;
    uint32_t write_lba_;
    uint8_t post_dcb_status_reads_;
    bool defer_completion_;
    bool defer_sense_;
    bool defer_init_;
    uint8_t init_params_[8];
    uint8_t init_n_;
    bool attached_;
    bool irq_latched_;
    uint32_t last_cyl_;
    uint32_t last_head_;
    uint32_t last_sec_;
    uint8_t last_drive_;
};

} // namespace iron86

#endif /* IRON86_HDC_H */
