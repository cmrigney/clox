#ifndef RF95_H
#define RF95_H

/// \brief Defines register values for a set of modem configuration registers
///
/// Defines register values for a set of modem configuration registers
/// that can be passed to setModemRegisters() if none of the choices in
/// ModemConfigChoice suit your need setModemRegisters() writes the
/// register values from this structure to the appropriate registers
/// to set the desired spreading factor, coding rate and bandwidth
typedef struct
{
  uint8_t reg_1d; ///< Value for register RH_RF95_REG_1D_MODEM_CONFIG1
  uint8_t reg_1e; ///< Value for register RH_RF95_REG_1E_MODEM_CONFIG2
  uint8_t reg_26; ///< Value for register RH_RF95_REG_26_MODEM_CONFIG3
} ModemConfig;

/// \brief Defines different operating modes for the transport hardware
///
/// These are the different values that can be adopted by the _mode variable and
/// returned by the mode() member function,
static const int RHModeInitialising = 0; ///< Transport is initialising. Initial default value until init() is called..
static const int RHModeSleep = 1;        ///< Transport hardware is in low power sleep mode (if supported)
static const int RHModeIdle = 2;         ///< Transport is idle.
static const int RHModeTx = 3;           ///< Transport is in the process of transmitting a message.
static const int RHModeRx = 4;           ///< Transport is in the process of receiving a message.
static const int RHModeCad = 5;          ///< Transport is in the process of detecting channel activity (if supported)

/// Choices for setModemConfig() for a selected subset of common
/// data rates. If you need another configuration,
/// determine the necessary settings and call setModemRegisters() with your
/// desired settings. It might be helpful to use the LoRa calculator mentioned in
/// http://www.semtech.com/images/datasheet/LoraDesignGuide_STD.pdf
/// These are indexes into MODEM_CONFIG_TABLE. We strongly recommend you use these symbolic
/// definitions and not their integer equivalents: its possible that new values will be
/// introduced in later versions (though we will try to avoid it).
/// Caution: if you are using slow packet rates and long packets with RHReliableDatagram or subclasses
/// you may need to change the RHReliableDatagram timeout for reliable operations.
/// Caution: for some slow rates nad with ReliableDatagrams you may need to increase the reply timeout
/// with manager.setTimeout() to
/// deal with the long transmission times.
/// Caution: SX1276 family errata suggests alternate settings for some LoRa registers when 500kHz bandwidth
/// is in use. See the Semtech SX1276/77/78 Errata Note. These are not implemented by RH_RF95.
static const int Bw125Cr45Sf128 = 0;   ///< Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on. Default medium range
static const int Bw500Cr45Sf128 = 1;   ///< Bw = 500 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on. Fast+short range
static const int Bw31_25Cr48Sf512 = 2; ///< Bw = 31.25 kHz, Cr = 4/8, Sf = 512chips/symbol, CRC on. Slow+long range
static const int Bw125Cr48Sf4096 = 3;  ///< Bw = 125 kHz, Cr = 4/8, Sf = 4096chips/symbol, low data rate, CRC on. Slow+long range
static const int Bw125Cr45Sf2048 = 4;  ///< Bw = 125 kHz, Cr = 4/5, Sf = 2048chips/symbol, CRC on. Slow+long range

// This is the bit in the SPI address that marks it as a write
#define RH_SPI_WRITE_MASK 0x80

// This is the maximum number of interrupts the driver can support
// Most Arduinos can handle 2, Megas can handle more
#define RH_RF95_NUM_INTERRUPTS 3

// Max number of octets the LORA Rx/Tx FIFO can hold
#define RH_RF95_FIFO_SIZE 255

// This is the maximum number of bytes that can be carried by the LORA.
// We use some for headers, keeping fewer for RadioHead messages
#define RH_RF95_MAX_PAYLOAD_LEN RH_RF95_FIFO_SIZE

// The length of the headers we add.
// The headers are inside the LORA's payload
#define RH_RF95_HEADER_LEN 4

// This is the maximum message length that can be supported by this driver.
// Can be pre-defined to a smaller size (to save SRAM) prior to including this header
// Here we allow for 1 byte message length, 4 bytes headers, user data and 2 bytes of FCS
#ifndef RH_RF95_MAX_MESSAGE_LEN
#define RH_RF95_MAX_MESSAGE_LEN (RH_RF95_MAX_PAYLOAD_LEN - RH_RF95_HEADER_LEN)
#endif

// The crystal oscillator frequency of the module
#define RH_RF95_FXOSC 32000000.0

// The Frequency Synthesizer step = RH_RF95_FXOSC / 2^^19
#define RH_RF95_FSTEP (RH_RF95_FXOSC / 524288)

// Register names (LoRa Mode, from table 85)
#define RH_RF95_REG_00_FIFO 0x00
#define RH_RF95_REG_01_OP_MODE 0x01
#define RH_RF95_REG_02_RESERVED 0x02
#define RH_RF95_REG_03_RESERVED 0x03
#define RH_RF95_REG_04_RESERVED 0x04
#define RH_RF95_REG_05_RESERVED 0x05
#define RH_RF95_REG_06_FRF_MSB 0x06
#define RH_RF95_REG_07_FRF_MID 0x07
#define RH_RF95_REG_08_FRF_LSB 0x08
#define RH_RF95_REG_09_PA_CONFIG 0x09
#define RH_RF95_REG_0A_PA_RAMP 0x0a
#define RH_RF95_REG_0B_OCP 0x0b
#define RH_RF95_REG_0C_LNA 0x0c
#define RH_RF95_REG_0D_FIFO_ADDR_PTR 0x0d
#define RH_RF95_REG_0E_FIFO_TX_BASE_ADDR 0x0e
#define RH_RF95_REG_0F_FIFO_RX_BASE_ADDR 0x0f
#define RH_RF95_REG_10_FIFO_RX_CURRENT_ADDR 0x10
#define RH_RF95_REG_11_IRQ_FLAGS_MASK 0x11
#define RH_RF95_REG_12_IRQ_FLAGS 0x12
#define RH_RF95_REG_13_RX_NB_BYTES 0x13
#define RH_RF95_REG_14_RX_HEADER_CNT_VALUE_MSB 0x14
#define RH_RF95_REG_15_RX_HEADER_CNT_VALUE_LSB 0x15
#define RH_RF95_REG_16_RX_PACKET_CNT_VALUE_MSB 0x16
#define RH_RF95_REG_17_RX_PACKET_CNT_VALUE_LSB 0x17
#define RH_RF95_REG_18_MODEM_STAT 0x18
#define RH_RF95_REG_19_PKT_SNR_VALUE 0x19
#define RH_RF95_REG_1A_PKT_RSSI_VALUE 0x1a
#define RH_RF95_REG_1B_RSSI_VALUE 0x1b
#define RH_RF95_REG_1C_HOP_CHANNEL 0x1c
#define RH_RF95_REG_1D_MODEM_CONFIG1 0x1d
#define RH_RF95_REG_1E_MODEM_CONFIG2 0x1e
#define RH_RF95_REG_1F_SYMB_TIMEOUT_LSB 0x1f
#define RH_RF95_REG_20_PREAMBLE_MSB 0x20
#define RH_RF95_REG_21_PREAMBLE_LSB 0x21
#define RH_RF95_REG_22_PAYLOAD_LENGTH 0x22
#define RH_RF95_REG_23_MAX_PAYLOAD_LENGTH 0x23
#define RH_RF95_REG_24_HOP_PERIOD 0x24
#define RH_RF95_REG_25_FIFO_RX_BYTE_ADDR 0x25
#define RH_RF95_REG_26_MODEM_CONFIG3 0x26

#define RH_RF95_REG_27_PPM_CORRECTION 0x27
#define RH_RF95_REG_28_FEI_MSB 0x28
#define RH_RF95_REG_29_FEI_MID 0x29
#define RH_RF95_REG_2A_FEI_LSB 0x2a
#define RH_RF95_REG_2C_RSSI_WIDEBAND 0x2c
#define RH_RF95_REG_31_DETECT_OPTIMIZE 0x31
#define RH_RF95_REG_33_INVERT_IQ 0x33
#define RH_RF95_REG_37_DETECTION_THRESHOLD 0x37
#define RH_RF95_REG_39_SYNC_WORD 0x39

#define RH_RF95_REG_40_DIO_MAPPING1 0x40
#define RH_RF95_REG_41_DIO_MAPPING2 0x41
#define RH_RF95_REG_42_VERSION 0x42

#define RH_RF95_REG_4B_TCXO 0x4b
#define RH_RF95_REG_4D_PA_DAC 0x4d
#define RH_RF95_REG_5B_FORMER_TEMP 0x5b
#define RH_RF95_REG_61_AGC_REF 0x61
#define RH_RF95_REG_62_AGC_THRESH1 0x62
#define RH_RF95_REG_63_AGC_THRESH2 0x63
#define RH_RF95_REG_64_AGC_THRESH3 0x64

// RH_RF95_REG_01_OP_MODE                             0x01
#define RH_RF95_LONG_RANGE_MODE 0x80
#define RH_RF95_ACCESS_SHARED_REG 0x40
#define RH_RF95_LOW_FREQUENCY_MODE 0x08
#define RH_RF95_MODE 0x07
#define RH_RF95_MODE_SLEEP 0x00
#define RH_RF95_MODE_STDBY 0x01
#define RH_RF95_MODE_FSTX 0x02
#define RH_RF95_MODE_TX 0x03
#define RH_RF95_MODE_FSRX 0x04
#define RH_RF95_MODE_RXCONTINUOUS 0x05
#define RH_RF95_MODE_RXSINGLE 0x06
#define RH_RF95_MODE_CAD 0x07

// RH_RF95_REG_09_PA_CONFIG                           0x09
#define RH_RF95_PA_SELECT 0x80
#define RH_RF95_MAX_POWER 0x70
#define RH_RF95_OUTPUT_POWER 0x0f

// RH_RF95_REG_0A_PA_RAMP                             0x0a
#define RH_RF95_LOW_PN_TX_PLL_OFF 0x10
#define RH_RF95_PA_RAMP 0x0f
#define RH_RF95_PA_RAMP_3_4MS 0x00
#define RH_RF95_PA_RAMP_2MS 0x01
#define RH_RF95_PA_RAMP_1MS 0x02
#define RH_RF95_PA_RAMP_500US 0x03
#define RH_RF95_PA_RAMP_250US 0x04
#define RH_RF95_PA_RAMP_125US 0x05
#define RH_RF95_PA_RAMP_100US 0x06
#define RH_RF95_PA_RAMP_62US 0x07
#define RH_RF95_PA_RAMP_50US 0x08
#define RH_RF95_PA_RAMP_40US 0x09
#define RH_RF95_PA_RAMP_31US 0x0a
#define RH_RF95_PA_RAMP_25US 0x0b
#define RH_RF95_PA_RAMP_20US 0x0c
#define RH_RF95_PA_RAMP_15US 0x0d
#define RH_RF95_PA_RAMP_12US 0x0e
#define RH_RF95_PA_RAMP_10US 0x0f

// RH_RF95_REG_0B_OCP                                 0x0b
#define RH_RF95_OCP_ON 0x20
#define RH_RF95_OCP_TRIM 0x1f

// RH_RF95_REG_0C_LNA                                 0x0c
#define RH_RF95_LNA_GAIN 0xe0
#define RH_RF95_LNA_GAIN_G1 0x20
#define RH_RF95_LNA_GAIN_G2 0x40
#define RH_RF95_LNA_GAIN_G3 0x60
#define RH_RF95_LNA_GAIN_G4 0x80
#define RH_RF95_LNA_GAIN_G5 0xa0
#define RH_RF95_LNA_GAIN_G6 0xc0
#define RH_RF95_LNA_BOOST_LF 0x18
#define RH_RF95_LNA_BOOST_LF_DEFAULT 0x00
#define RH_RF95_LNA_BOOST_HF 0x03
#define RH_RF95_LNA_BOOST_HF_DEFAULT 0x00
#define RH_RF95_LNA_BOOST_HF_150PC 0x03

// RH_RF95_REG_11_IRQ_FLAGS_MASK                      0x11
#define RH_RF95_RX_TIMEOUT_MASK 0x80
#define RH_RF95_RX_DONE_MASK 0x40
#define RH_RF95_PAYLOAD_CRC_ERROR_MASK 0x20
#define RH_RF95_VALID_HEADER_MASK 0x10
#define RH_RF95_TX_DONE_MASK 0x08
#define RH_RF95_CAD_DONE_MASK 0x04
#define RH_RF95_FHSS_CHANGE_CHANNEL_MASK 0x02
#define RH_RF95_CAD_DETECTED_MASK 0x01

// RH_RF95_REG_12_IRQ_FLAGS                           0x12
#define RH_RF95_RX_TIMEOUT 0x80
#define RH_RF95_RX_DONE 0x40
#define RH_RF95_PAYLOAD_CRC_ERROR 0x20
#define RH_RF95_VALID_HEADER 0x10
#define RH_RF95_TX_DONE 0x08
#define RH_RF95_CAD_DONE 0x04
#define RH_RF95_FHSS_CHANGE_CHANNEL 0x02
#define RH_RF95_CAD_DETECTED 0x01

// RH_RF95_REG_18_MODEM_STAT                          0x18
#define RH_RF95_RX_CODING_RATE 0xe0
#define RH_RF95_MODEM_STATUS_CLEAR 0x10
#define RH_RF95_MODEM_STATUS_HEADER_INFO_VALID 0x08
#define RH_RF95_MODEM_STATUS_RX_ONGOING 0x04
#define RH_RF95_MODEM_STATUS_SIGNAL_SYNCHRONIZED 0x02
#define RH_RF95_MODEM_STATUS_SIGNAL_DETECTED 0x01

// RH_RF95_REG_1C_HOP_CHANNEL                         0x1c
#define RH_RF95_PLL_TIMEOUT 0x80
#define RH_RF95_RX_PAYLOAD_CRC_IS_ON 0x40
#define RH_RF95_FHSS_PRESENT_CHANNEL 0x3f

// RH_RF95_REG_1D_MODEM_CONFIG1                       0x1d
#define RH_RF95_BW 0xf0

#define RH_RF95_BW_7_8KHZ 0x00
#define RH_RF95_BW_10_4KHZ 0x10
#define RH_RF95_BW_15_6KHZ 0x20
#define RH_RF95_BW_20_8KHZ 0x30
#define RH_RF95_BW_31_25KHZ 0x40
#define RH_RF95_BW_41_7KHZ 0x50
#define RH_RF95_BW_62_5KHZ 0x60
#define RH_RF95_BW_125KHZ 0x70
#define RH_RF95_BW_250KHZ 0x80
#define RH_RF95_BW_500KHZ 0x90
#define RH_RF95_CODING_RATE 0x0e
#define RH_RF95_CODING_RATE_4_5 0x02
#define RH_RF95_CODING_RATE_4_6 0x04
#define RH_RF95_CODING_RATE_4_7 0x06
#define RH_RF95_CODING_RATE_4_8 0x08
#define RH_RF95_IMPLICIT_HEADER_MODE_ON 0x01

// RH_RF95_REG_1E_MODEM_CONFIG2                       0x1e
#define RH_RF95_SPREADING_FACTOR 0xf0
#define RH_RF95_SPREADING_FACTOR_64CPS 0x60
#define RH_RF95_SPREADING_FACTOR_128CPS 0x70
#define RH_RF95_SPREADING_FACTOR_256CPS 0x80
#define RH_RF95_SPREADING_FACTOR_512CPS 0x90
#define RH_RF95_SPREADING_FACTOR_1024CPS 0xa0
#define RH_RF95_SPREADING_FACTOR_2048CPS 0xb0
#define RH_RF95_SPREADING_FACTOR_4096CPS 0xc0
#define RH_RF95_TX_CONTINUOUS_MODE 0x08

#define RH_RF95_PAYLOAD_CRC_ON 0x04
#define RH_RF95_SYM_TIMEOUT_MSB 0x03

// RH_RF95_REG_26_MODEM_CONFIG3
#define RH_RF95_MOBILE_NODE 0x08            // HopeRF term
#define RH_RF95_LOW_DATA_RATE_OPTIMIZE 0x08 // Semtechs term
#define RH_RF95_AGC_AUTO_ON 0x04

// RH_RF95_REG_4B_TCXO                                0x4b
#define RH_RF95_TCXO_TCXO_INPUT_ON 0x10

// RH_RF95_REG_4D_PA_DAC                              0x4d
#define RH_RF95_PA_DAC_DISABLE 0x04
#define RH_RF95_PA_DAC_ENABLE 0x07

// This is the address that indicates a broadcast
#define RH_BROADCAST_ADDRESS 0xff

typedef struct RF95
{
  uint cs_pin;
  uint reset_pin;
  uint interrupt_pin;

  volatile int mode;
  /// True if we are using the HF port (779.0 MHz and above)
  bool usingHFport;
  /// False if the PA_BOOST transmitter output pin is to be used.
  /// True if the RFO transmitter output pin is to be used.
  bool useRFO;
  /// If true, sends CRCs in every packet and requires a valid CRC in every received packet
  bool enableCRC;
  /// True when there is a valid message in the buffer
  volatile bool rxBufValid;
  /// Number of octets in the buffer
  volatile uint8_t bufLen;
  /// The receiver/transmitter buffer
  uint8_t buf[RH_RF95_MAX_PAYLOAD_LEN];

  /// Last measured SNR, dB
  int8_t lastSNR;
  /// The value of the last received RSSI value, in some transport specific units
  volatile int16_t lastRssi;
  /// TO header in the last received mesasge
  volatile uint8_t rxHeaderTo;

  /// FROM header in the last received mesasge
  volatile uint8_t rxHeaderFrom;

  /// ID header in the last received mesasge
  volatile uint8_t rxHeaderId;

  /// FLAGS header in the last received mesasge
  volatile uint8_t rxHeaderFlags;

  /// TO header to send in all messages
  uint8_t txHeaderTo;

  /// FROM header to send in all messages
  uint8_t txHeaderFrom;

  /// ID header to send in all messages
  uint8_t txHeaderId;

  /// FLAGS header to send in all messages
  uint8_t txHeaderFlags;

  /// This node id
  uint8_t thisAddress;

  /// Count of the number of bad messages (eg bad checksum etc) received
  volatile uint16_t rxBad;

  /// Count of the number of successfully transmitted messaged
  volatile uint16_t rxGood;

  /// Count of the number of bad messages (correct checksum etc) received
  volatile uint16_t txGood;

  /// Channel activity detected
  volatile bool cad;

  /// Channel activity timeout in ms
  unsigned int cad_timeout;

  /// device ID
  uint8_t deviceVersion;

  /// Whether the transport is in promiscuous mode
  bool promiscuous;

} RF95;

RF95 *rf95_create(uint reset_pin, uint interrupt_pin);
bool rf95_setModemConfig(RF95 *rf95, int index);
bool rf95_setFrequency(RF95 *rf95, float centre);
void rf95_setTxPower(RF95 *rf95, int8_t power);
bool rf95_available(RF95 *rf95);
bool rf95_recv(RF95 *rf95, uint8_t *buf, uint8_t *len);
bool rf95_waitPacketSent(RF95 *rf95);
bool rf95_waitPacketSentTimeout(RF95 *rf95, uint16_t timeout);
bool rf95_send(RF95 *rf95, const uint8_t *data, uint8_t len);
void rf95_deinit(RF95 *rf95);
void rf95_free(RF95 *rf95);
void rf95_setModeIdle(RF95 *rf95);

#endif