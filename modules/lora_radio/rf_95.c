// Adapted from RadioHead library, should be compatible.
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "hardware/sync.h"
#include "rf_95.h"
#include "../clox.h"

void rf95_interrupt_handler();
void rf95_setModeIdle(RF95 *rf95);
void rf95_setPreambleLength(RF95 *rf95, uint16_t bytes);

// only one device supported for now
static RF95 *rf95_deviceForInterrupt = NULL;

// These are indexed by the values of ModemConfigChoice
// Stored in flash (program) memory to save SRAM
__in_flash() static const ModemConfig MODEM_CONFIG_TABLE[] =
    {
        //  1d,     1e,      26
        {0x72, 0x74, 0x04}, // Bw125Cr45Sf128 (the chip default), AGC enabled
        {0x92, 0x74, 0x04}, // Bw500Cr45Sf128, AGC enabled
        {0x48, 0x94, 0x04}, // Bw31_25Cr48Sf512, AGC enabled
        {0x78, 0xc4, 0x0c}, // Bw125Cr48Sf4096, AGC enabled
        {0x72, 0xb4, 0x04}, // Bw125Cr45Sf2048, AGC enabled
};

#define ATOMIC_BLOCK_START uint32_t rf95_interrupts = save_and_disable_interrupts()
#define ATOMIC_BLOCK_END restore_interrupts(rf95_interrupts)

static inline unsigned long millis() {
  return ((double)clock() / CLOCKS_PER_SEC) * 1000;
}

static inline void cs_select(RF95 *rf95)
{
  asm volatile("nop \n nop \n nop");
  gpio_put(rf95->cs_pin, 0); // Active low
  asm volatile("nop \n nop \n nop");
}

static inline void cs_deselect(RF95 *rf95)
{
  asm volatile("nop \n nop \n nop");
  gpio_put(rf95->cs_pin, 1);
  asm volatile("nop \n nop \n nop");
}

static uint8_t spiWrite(RF95 *rf95, uint8_t reg, uint8_t val)
{
  uint8_t data[2];
  data[0] = reg | RH_SPI_WRITE_MASK;
  data[1] = val;
  uint8_t dest[2];
  ATOMIC_BLOCK_START;
  cs_select(rf95);
  spi_write_read_blocking(spi_default, data, dest, 2);
  busy_wait_us(1);
  cs_deselect(rf95);
  ATOMIC_BLOCK_END;
  return dest[0]; // status
}

static uint8_t spiBurstWrite(RF95 *rf95, uint8_t reg, const uint8_t *src, uint8_t len)
{
  uint8_t scratch[len];
  uint8_t status;
  uint8_t data = reg | RH_SPI_WRITE_MASK;
  ATOMIC_BLOCK_START;
  cs_select(rf95);
  spi_write_read_blocking(spi_default, &data, &status, 1);
  spi_write_read_blocking(spi_default, src, scratch, len);
  cs_deselect(rf95);
  ATOMIC_BLOCK_END;
  return status;
}

static uint8_t spiRead(RF95 *rf95, uint8_t reg)
{
  uint8_t data[2];
  data[0] = reg & ~RH_SPI_WRITE_MASK;
  data[1] = 0;
  uint8_t dest[2];
  memset(dest, 0, 2);
  ATOMIC_BLOCK_START;
  cs_select(rf95);
  spi_write_read_blocking(spi_default, data, dest, 2);
  cs_deselect(rf95);
  ATOMIC_BLOCK_END;
  return dest[1]; // value
}

static uint8_t spiBurstRead(RF95 *rf95, uint8_t reg, uint8_t *dest, uint8_t len)
{
  uint8_t scratch[len];
  memset(scratch, 0, len);
  uint8_t status;
  uint8_t data = reg & ~RH_SPI_WRITE_MASK;
  ATOMIC_BLOCK_START;
  cs_select(rf95);
  spi_write_read_blocking(spi_default, &data, &status, 1);
  spi_write_read_blocking(spi_default, scratch, dest, len);
  cs_deselect(rf95);
  ATOMIC_BLOCK_END;
  return status;
}

static void rf95_reset(RF95 *rf95) {
  gpio_put(rf95->reset_pin, 0);
  sleep_us(100);
  gpio_put(rf95->reset_pin, 1);
  sleep_ms(5);
}

RF95 *rf95_create(uint reset_pin, uint interrupt_pin)
{
  RF95 *rf95 = ALLOCATE(RF95, 1);
  memset(rf95, 0, sizeof(RF95));
  rf95->cs_pin = PICO_DEFAULT_SPI_CSN_PIN;
  rf95->reset_pin = reset_pin;
  rf95->interrupt_pin = interrupt_pin;
  rf95->mode = RHModeInitialising;

  gpio_init(rf95->reset_pin);
  gpio_init(rf95->cs_pin);

  spi_init(spi_default, 1000 * 1000); // 1 Mhz

  // Set SPI format
  spi_set_format(spi_default,   // SPI instance
                  8,      // Number of bits per transfer
                  0,      // Polarity (CPOL)
                  0,      // Phase (CPHA)
                  SPI_MSB_FIRST);

  gpio_set_function(PICO_DEFAULT_SPI_RX_PIN, GPIO_FUNC_SPI);
  gpio_set_function(PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI);
  gpio_set_function(PICO_DEFAULT_SPI_TX_PIN, GPIO_FUNC_SPI);
  // TODO is it ok to control via hardware? See https://forums.raspberrypi.com/viewtopic.php?t=309859
  // gpio_set_function(PICO_DEFAULT_SPI_CSN_PIN, GPIO_FUNC_SPI);

  gpio_set_dir(rf95->cs_pin, GPIO_OUT);
  gpio_set_dir(rf95->reset_pin, GPIO_OUT);

  cs_deselect(rf95);

  rf95_reset(rf95);

  sleep_ms(100);

  uint8_t version = spiRead(rf95, RH_RF95_REG_42_VERSION);
  if(version != 18) {
    perror("Failed to find rfm9x with expected version\n");
    spi_deinit(spi_default);
    FREE(RF95, rf95);
    return NULL;
  }

  // Set sleep mode, so we can also set LORA mode:
  spiWrite(rf95, RH_RF95_REG_01_OP_MODE, RH_RF95_MODE_SLEEP | RH_RF95_LONG_RANGE_MODE);
  sleep_ms(10); // Wait for sleep mode to take over from say, CAD
  // Check we are in sleep mode, with LORA set
  if (spiRead(rf95, RH_RF95_REG_01_OP_MODE) != (RH_RF95_MODE_SLEEP | RH_RF95_LONG_RANGE_MODE))
  {
    perror("Failed to configure rfm9x module\n");
    spi_deinit(spi_default);
    FREE(RF95, rf95);
    return NULL; // No device present?
  }

  rf95_deviceForInterrupt = rf95;
  // TODO might prefer raw handler instead: https://raspberrypi.github.io/pico-sdk-doxygen/group__hardware__gpio.html#ga2e78fcd487a3a2e173322c6502fe9419
  gpio_set_irq_enabled_with_callback(rf95->interrupt_pin, GPIO_IRQ_EDGE_RISE, true, &rf95_interrupt_handler);

  // Set up FIFO
  // We configure so that we can use the entire 256 byte FIFO for either receive
  // or transmit, but not both at the same time
  spiWrite(rf95, RH_RF95_REG_0E_FIFO_TX_BASE_ADDR, 0);
  spiWrite(rf95, RH_RF95_REG_0F_FIFO_RX_BASE_ADDR, 0);

  // Packet format is preamble + explicit-header + payload + crc
  // Explicit Header Mode
  // payload is TO + FROM + ID + FLAGS + message data
  // RX mode is implmented with RXCONTINUOUS
  // max message data length is 255 - 4 = 251 octets

  rf95_setModeIdle(rf95);

  // Set up default configuration
  // No Sync Words in LORA mode.
  rf95_setModemConfig(rf95, Bw125Cr45Sf128); // Radio default
                                        //    setModemConfig(Bw125Cr48Sf4096); // slow and reliable?
  rf95_setPreambleLength(rf95, 8);           // Default is 8
  
  rf95_setFrequency(rf95, 915.0);
  // Lowish power
  rf95_setTxPower(rf95, 13);

  return rf95;
}

void rf95_setModeIdle(RF95 *rf95)
{
  if (rf95->mode != RHModeIdle)
  {
    // modeWillChange(RHModeIdle);
    spiWrite(rf95, RH_RF95_REG_01_OP_MODE, RH_RF95_MODE_STDBY);
    rf95->mode = RHModeIdle;
  }
}

// Sets registers from a canned modem configuration structure
void rf95_setModemRegisters(RF95 *rf95, const ModemConfig *config)
{
  spiWrite(rf95, RH_RF95_REG_1D_MODEM_CONFIG1, config->reg_1d);
  spiWrite(rf95, RH_RF95_REG_1E_MODEM_CONFIG2, config->reg_1e);
  spiWrite(rf95, RH_RF95_REG_26_MODEM_CONFIG3, config->reg_26);
}

// Returns true if its a valid choice
bool rf95_setModemConfig(RF95 *rf95, int index)
{
  if (index > (signed int)(sizeof(MODEM_CONFIG_TABLE) / sizeof(ModemConfig)))
    return false;

  ModemConfig cfg;
  memcpy(&cfg, &MODEM_CONFIG_TABLE[index], sizeof(ModemConfig));
  rf95_setModemRegisters(rf95, &cfg);

  return true;
}

void rf95_setPreambleLength(RF95 *rf95, uint16_t bytes)
{
  spiWrite(rf95, RH_RF95_REG_20_PREAMBLE_MSB, bytes >> 8);
  spiWrite(rf95, RH_RF95_REG_21_PREAMBLE_LSB, bytes & 0xff);
}

bool rf95_setFrequency(RF95 *rf95, float centre)
{
  // Frf = FRF / FSTEP
  uint32_t frf = (centre * 1000000.0) / RH_RF95_FSTEP;
  spiWrite(rf95, RH_RF95_REG_06_FRF_MSB, (frf >> 16) & 0xff);
  spiWrite(rf95, RH_RF95_REG_07_FRF_MID, (frf >> 8) & 0xff);
  spiWrite(rf95, RH_RF95_REG_08_FRF_LSB, frf & 0xff);
  rf95->usingHFport = (centre >= 779.0);

  return true;
}

void rf95_setTxPower(RF95 *rf95, int8_t power)
{
  rf95->useRFO = false;

  // Sigh, different behaviours depending on whether the module use PA_BOOST or the RFO pin
  // for the transmitter output
  if (rf95->useRFO)
  {
    if (power > 15)
      power = 15;
    if (power < 0)
      power = 0;
    // Set the MaxPower register to 0x7 => MaxPower = 10.8 + 0.6 * 7 = 15dBm
    // So Pout = Pmax - (15 - power) = 15 - 15 + power
    spiWrite(rf95, RH_RF95_REG_09_PA_CONFIG, RH_RF95_MAX_POWER | power);
    spiWrite(rf95, RH_RF95_REG_4D_PA_DAC, RH_RF95_PA_DAC_DISABLE);
  }
  else
  {
    if (power > 20)
      power = 20;
    if (power < 2)
      power = 2;

    // For RH_RF95_PA_DAC_ENABLE, manual says '+20dBm on PA_BOOST when OutputPower=0xf'
    // RH_RF95_PA_DAC_ENABLE actually adds about 3dBm to all power levels. We will use it
    // for 8, 19 and 20dBm
    if (power > 17)
    {
      spiWrite(rf95, RH_RF95_REG_4D_PA_DAC, RH_RF95_PA_DAC_ENABLE);
      power -= 3;
    }
    else
    {
      spiWrite(rf95, RH_RF95_REG_4D_PA_DAC, RH_RF95_PA_DAC_DISABLE);
    }

    // RFM95/96/97/98 does not have RFO pins connected to anything. Only PA_BOOST
    // pin is connected, so must use PA_BOOST
    // Pout = 2 + OutputPower (+3dBm if DAC enabled)
    spiWrite(rf95, RH_RF95_REG_09_PA_CONFIG, RH_RF95_PA_SELECT | (power - 2));
  }
}

void rf95_setModeRx(RF95 *rf95)
{
  if (rf95->mode != RHModeRx)
  {
    // modeWillChange(RHModeRx);
    spiWrite(rf95, RH_RF95_REG_01_OP_MODE, RH_RF95_MODE_RXCONTINUOUS);
    spiWrite(rf95, RH_RF95_REG_40_DIO_MAPPING1, 0x00); // Interrupt on RxDone
    rf95->mode = RHModeRx;
  }
}

void rf95_setModeTx(RF95 *rf95)
{
  if (rf95->mode != RHModeTx)
  {
    // modeWillChange(RHModeTx);
    spiWrite(rf95, RH_RF95_REG_01_OP_MODE, RH_RF95_MODE_TX);
    spiWrite(rf95, RH_RF95_REG_40_DIO_MAPPING1, 0x40); // Interrupt on TxDone
    rf95->mode = RHModeTx;
  }
}

void rf95_clearRxBuf(RF95 *rf95)
{
  ATOMIC_BLOCK_START;
  rf95->rxBufValid = false;
  rf95->bufLen = 0;
  ATOMIC_BLOCK_END;
}

// Check whether the latest received message is complete and uncorrupted
void rf95_validateRxBuf(RF95 *rf95)
{
  if (rf95->bufLen < 4)
    return; // Too short to be a real message
  // Extract the 4 headers
  rf95->rxHeaderTo = rf95->buf[0];
  rf95->rxHeaderFrom = rf95->buf[1];
  rf95->rxHeaderId = rf95->buf[2];
  rf95->rxHeaderFlags = rf95->buf[3];
  if (rf95->promiscuous ||
      rf95->rxHeaderTo == rf95->thisAddress ||
      rf95->rxHeaderTo == RH_BROADCAST_ADDRESS)
  {
    rf95->rxGood++;
    rf95->rxBufValid = true;
  }
}

void rf95_interrupt_handler()
{
  RF95 *rf95 = rf95_deviceForInterrupt;
  if (rf95 == NULL)
  {
    // something went terribly wrong
    return;
  }
  // Read the interrupt register
  uint8_t irq_flags = spiRead(rf95, RH_RF95_REG_12_IRQ_FLAGS);
  // Read the RegHopChannel register to check if CRC presence is signalled
  // in the header. If not it might be a stray (noise) packet.*
  uint8_t hop_channel = spiRead(rf95, RH_RF95_REG_1C_HOP_CHANNEL);

  // ack all interrupts,
  spiWrite(rf95, RH_RF95_REG_12_IRQ_FLAGS, 0xff); // Clear all IRQ flags

  // error if:
  // timeout
  // bad CRC
  // CRC is required but it is not present
  if (rf95->mode == RHModeRx && ((irq_flags & (RH_RF95_RX_TIMEOUT | RH_RF95_PAYLOAD_CRC_ERROR)) || (rf95->enableCRC && !(hop_channel & RH_RF95_RX_PAYLOAD_CRC_IS_ON))))
  //    if (_mode == RHModeRx && irq_flags & (RH_RF95_RX_TIMEOUT | RH_RF95_PAYLOAD_CRC_ERROR))
  {
    rf95->rxBad++;
    rf95_clearRxBuf(rf95);
  }
  // It is possible to get RX_DONE and CRC_ERROR and VALID_HEADER all at once
  // so this must be an else
  else if (rf95->mode == RHModeRx && irq_flags & RH_RF95_RX_DONE)
  {
    // Packet received, no CRC error
    // Have received a packet
    uint8_t len = spiRead(rf95, RH_RF95_REG_13_RX_NB_BYTES);

    // Reset the fifo read ptr to the beginning of the packet
    spiWrite(rf95, RH_RF95_REG_0D_FIFO_ADDR_PTR, spiRead(rf95, RH_RF95_REG_10_FIFO_RX_CURRENT_ADDR));
    spiBurstRead(rf95, RH_RF95_REG_00_FIFO, rf95->buf, len);
    rf95->bufLen = len;

    // Remember the last signal to noise ratio, LORA mode
    // Per page 111, SX1276/77/78/79 datasheet
    rf95->lastSNR = (int8_t)spiRead(rf95, RH_RF95_REG_19_PKT_SNR_VALUE) / 4;

    // Remember the RSSI of this packet, LORA mode
    // this is according to the doc, but is it really correct?
    // weakest receiveable signals are reported RSSI at about -66
    rf95->lastRssi = spiRead(rf95, RH_RF95_REG_1A_PKT_RSSI_VALUE);
    // Adjust the RSSI, datasheet page 87
    if (rf95->lastSNR < 0)
      rf95->lastRssi = rf95->lastRssi + rf95->lastSNR;
    else
      rf95->lastRssi = (int)rf95->lastRssi * 16 / 15;
    if (rf95->usingHFport)
      rf95->lastRssi -= 157;
    else
      rf95->lastRssi -= 164;

    // We have received a message.
    rf95_validateRxBuf(rf95);
    if (rf95->rxBufValid)
      rf95_setModeIdle(rf95); // Got one
  }
  else if (rf95->mode == RHModeTx && irq_flags & RH_RF95_TX_DONE)
  {
    //	Serial.println("T");
    rf95->txGood++;
    rf95_setModeIdle(rf95);
  }
  else if (rf95->mode == RHModeCad && irq_flags & RH_RF95_CAD_DONE)
  {
    //	Serial.println("C");
    rf95->cad = irq_flags & RH_RF95_CAD_DETECTED;
    rf95_setModeIdle(rf95);
  }
  else
  {
    //	Serial.println("?");
  }

  // Sigh: on some processors, for some unknown reason, doing this only once does not actually
  // clear the radio's interrupt flag. So we do it twice. Why?
  spiWrite(rf95, RH_RF95_REG_12_IRQ_FLAGS, 0xff); // Clear all IRQ flags
  spiWrite(rf95, RH_RF95_REG_12_IRQ_FLAGS, 0xff); // Clear all IRQ flags
}

bool rf95_available(RF95 *rf95)
{
  if (rf95->mode == RHModeTx)
  {
    return false;
  }
  rf95_setModeRx(rf95);
  return rf95->rxBufValid; // Will be set by the interrupt handler when a good message is received
}

bool rf95_recv(RF95 *rf95, uint8_t *buf, uint8_t *len)
{
  if (!rf95_available(rf95))
    return false;
  if (buf && len)
  {
    ATOMIC_BLOCK_START;
    // Skip the 4 headers that are at the beginning of the rxBuf
    if (*len > rf95->bufLen - RH_RF95_HEADER_LEN)
      *len = rf95->bufLen - RH_RF95_HEADER_LEN;
    memcpy(buf, rf95->buf + RH_RF95_HEADER_LEN, *len);
    ATOMIC_BLOCK_END;
  }
  rf95_clearRxBuf(rf95); // This message accepted and cleared
  return true;
}

bool rf95_waitPacketSent(RF95 *rf95)
{
  while (rf95->mode == RHModeTx)
    sleep_us(50); // Wait for any previous transmit to finish
  return true;
}

bool rf95_waitPacketSentTimeout(RF95 *rf95, uint16_t timeout)
{
  unsigned long starttime = clock();
  while ((millis() - starttime) < timeout)
  {
    if (rf95->mode != RHModeTx) // Any previous transmit finished?
      return true;
    sleep_us(50);
  }
  return false;
}

bool rf95_isChannelActive(RF95 *rf95)
{
  // Set mode RHModeCad
  if (rf95->mode != RHModeCad)
  {
    // modeWillChange(RHModeCad);
    spiWrite(rf95, RH_RF95_REG_01_OP_MODE, RH_RF95_MODE_CAD);
    spiWrite(rf95, RH_RF95_REG_40_DIO_MAPPING1, 0x80); // Interrupt on CadDone
    rf95->mode = RHModeCad;
  }

  while (rf95->mode == RHModeCad)
    sleep_us(10);

  return rf95->cad;
}

// Wait until no channel activity detected or timeout
bool rf95_waitCAD(RF95 *rf95)
{
  if (!rf95->cad_timeout)
    return true;

  // Wait for any channel activity to finish or timeout
  // Sophisticated DCF function...
  // DCF : BackoffTime = random() x aSlotTime
  // 100 - 1000 ms
  // 10 sec timeout
  unsigned long t = millis();
  while (rf95_isChannelActive(rf95))
  {
    if (millis() - t > rf95->cad_timeout)
      return false;
    sleep_ms(200);
  }

  return true;
}

bool rf95_send(RF95 *rf95, const uint8_t *data, uint8_t len)
{
  if (len > RH_RF95_MAX_MESSAGE_LEN)
    return false;

  rf95_waitPacketSent(rf95); // Make sure we dont interrupt an outgoing message
  rf95_setModeIdle(rf95);

  if (!rf95_waitCAD(rf95))
    return false; // Check channel activity

  // Position at the beginning of the FIFO
  spiWrite(rf95, RH_RF95_REG_0D_FIFO_ADDR_PTR, 0);
  // The headers
  spiWrite(rf95, RH_RF95_REG_00_FIFO, rf95->txHeaderTo);
  spiWrite(rf95, RH_RF95_REG_00_FIFO, rf95->txHeaderFrom);
  spiWrite(rf95, RH_RF95_REG_00_FIFO, rf95->txHeaderId);
  spiWrite(rf95, RH_RF95_REG_00_FIFO, rf95->txHeaderFlags);
  // The message data
  spiBurstWrite(rf95, RH_RF95_REG_00_FIFO, data, len);
  spiWrite(rf95, RH_RF95_REG_22_PAYLOAD_LENGTH, len + RH_RF95_HEADER_LEN);

  rf95_setModeTx(rf95); // Start the transmitter

  // when Tx is done, interruptHandler will fire and radio mode will return to STANDBY
  return true;
}

void rf95_setPayloadCRC(RF95 *rf95, bool on)
{
  // Payload CRC is bit 2 of register 1E
  uint8_t current = spiRead(rf95, RH_RF95_REG_1E_MODEM_CONFIG2) & ~RH_RF95_PAYLOAD_CRC_ON; // mask off the CRC

  if (on)
    spiWrite(rf95, RH_RF95_REG_1E_MODEM_CONFIG2, current | RH_RF95_PAYLOAD_CRC_ON);
  else
    spiWrite(rf95, RH_RF95_REG_1E_MODEM_CONFIG2, current);
  rf95->enableCRC = on;
}

uint8_t rf95_getDeviceVersion(RF95 *rf95)
{
  rf95->deviceVersion = spiRead(rf95, RH_RF95_REG_42_VERSION);
  return rf95->deviceVersion;
}

void rf95_setHeaderFlags(RF95 *rf95, uint8_t set, uint8_t clear)
{
    rf95->txHeaderFlags &= ~clear;
    rf95->txHeaderFlags |= set;
}

void rf95_deinit(RF95 *rf95) {
  gpio_set_irq_enabled_with_callback(rf95->interrupt_pin, GPIO_IRQ_EDGE_RISE, false, NULL);
  rf95_deviceForInterrupt = NULL;
  spi_deinit(spi_default);
}

void rf95_free(RF95 *rf95) {
  rf95_deinit(rf95);
  FREE(RF95, rf95);
}