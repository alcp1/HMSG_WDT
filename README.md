# HMSG_WDT
Raspberry Pi C code for HMSG HAT: External watchdog power cycling via I2C interface.

## ENABLING PWM
- STEP 1: Edit config file to enable PWM:
```bash
sudo nano /boot/firmware/config.txt
```
- STEP 2: Add the following lines to the end of the file:
```text
dtoverlay=pwm-2chan,pin=12,func=4,pin2=13,func2=4
```
> REMARK: See the PWM Pins and LEDs below:

| LED on the HAT | Raspberry Pi PWM Channel | Raspberry Pi GPIO | Controlled by (SW)| Header Pin|
|-|-|-|-|-|
|LED2|PWM0|GPIO12|HMSG_WDT|32|
|LED3|PWM1|GPIO13|HMSG|33|

## ENABLING I2C
- STEP 1: Open the terminal and run: 
```bash
sudo raspi-config
```
- STEP 2: Navigate to Interface Options (or Interfacing Options). Select **I2C** and choose **Yes** when prompted to enable the ARM I2C interface.
- STEP 3: Reboot the Raspberry Pi (if not prompted on the screen)
```bash
sudo reboot
```
- STEP 4: Edit config file to enable PWM:
```bash
sudo nano /boot/firmware/config.txt
```
- STEP 5: Modify the line with i2c_arm=on to change the baud rate:
```text
dtparam=i2c_arm=on,i2c_arm_baudrate=20000
```
- STEP 6: Reboot the Raspberry Pi (if not prompted on the screen)
```bash
sudo reboot
```
### I2C Verification
- STEP 1: Install the i2c-tools package 
```bash
sudo apt install i2c-tools
```
- STEP 2: Verify that the I2C interface is active:
```bash
ls /dev/i2c*
```
_You should see: "/dev/i2c-1_"
- STEP 3: Detect I2C devices:
```bash
sudo i2cdetect -y 1
```
_This command will display a grid with any connected I2C devices. ATtiny on the HAT should appear on address 0x1E._

> REMARK: See below the I2C Pins used by the HAT:

| Raspberry Pi GPIO | Function | Header Pin | ATtiny402 Port | ATtiny402 Pin |
|-|-|-|-|-|
|GPIO2|SDA1|3|PA1|4|
|GPIO3|SCL1|5|PA2|5|

## SERIAL PORT CONFIGURATION
The Raspberry Pi TXD0 and RXD0 are connected on the HAT to the UPDI pin of the ATtiny402 for programming it with **pymcuprog** tool.
In order to do so, the Raspberry Pi serial port has to be configured in a specific way.

### 1. Disable Serial Console
Prevent the Linux kernel from using the serial port as a login terminal:
- STEP 1: Open the terminal and run: 
```bash
sudo raspi-config
```
- STEP 2: Navigate to Interface Options (or Interfacing Options). Select **Serial Port**. Select **No** when asked: _"Would you like a login shell to be accessible over serial?"_. Select **Yes** when asked: _"Would you like the serial port hardware to be enabled?"_

- STEP 3: Reboot the Raspberry Pi (if not prompted on the screen)
```bash
sudo reboot
```
 
### 2. Set for High Precision (PL011 UART)
By default on Raspberry Pi 3, 4, and Zero W, the high-performance PL011 UART is tied to the Bluetooth module, while the GPIO pins use the less stable "mini-UART" (ttyS0). To use the high-precision PL011 on pins 14/15, we must disable Bluetooth or swap the ports.
 
#### Option A: Disable Bluetooth (Highest Precision)
This completely frees the PL011 for GPIO pins 14 and 15.
- STEP 1: Edit config file:
```bash
sudo nano /boot/firmware/config.txt
```
- STEP 2: Add the following lines to the end of the file:
```text
dtoverlay=disable-bt
```
- STEP 3: Disable the Bluetooth modem service. Run on terminal:
```bash
sudo systemctl disable hciuart
```

#### Option B: Swap Bluetooth to Mini-UART
This keeps Bluetooth working but moves it to the less stable port, giving the high-precision PL011 to the GPIO pins.
- STEP 1: Edit config file:
```bash
sudo nano /boot/firmware/config.txt
```
- STEP 2: Add the following lines to the end of the file:
```
dtoverlay=miniuart-bt
```

### 3. Verification
- STEP 1: Reboot the Raspberry Pi
```bash
sudo reboot
```
- STEP 2: Check serial port mapping
```bash
ls -l /dev/serial*
```
_This command should show serial0 -> ttyAMA0 indicating the pins are now using the high-precision hardware._

## ATTINY PROGRAMMING
- STEP 1: Install Python3 and Pip
```bash
sudo apt update
sudo apt install python3-pip python3-venv -y
```
- STEP 2: Create a Virtual Environment
```bash
python3 -m venv updi_env
source updi_env/bin/activate
```
_You will notice your terminal prompt changes to show (updi_env), meaning any Python packages you install now will live safely inside this folder._
- STEP 3: Install pymcuprog
```bash
pip install pymcuprog
```

### Programming Environment
Whenever you open a fresh terminal window on your Raspberry Pi and want to program your ATtiny402 again, you just need to jump back into that environment by running:
```bash
source ~/updi_env/bin/activate
```
If you ever want to leave the environment and go back to normal terminal mode, simply type:
```bash
deactivate
```

### pymcuprog commands
- Ping the Device (Check Connection):
```bash
pymcuprog ping -d attiny402 -t uart -u /dev/ttyAMA0
```
_If successful, it will return the device ID and confirm that it responded._
- Write a Hex File:
```bash
pymcuprog write -d attiny402 -t uart -u /dev/ttyAMA0 -f firmware.hex
```
- Erase the Chip
```bash
pymcuprog erase -d attiny402 -t uart -u /dev/ttyAMA0
```
> REMARK: pymcuprog defaults to 115200 baud for serial UPDI. If your wires are long or catching interference, you can slow it down by appending --clk 19200 to your command.

## HW Tests
### PWM Tests
- STEP 1: Export Both PWM Channels. First, navigate to the system PWM directory. Because your overlay handles both channels under a single controller (pwmchip0), you need to tell the kernel to open control interfaces for both 0 and 1.
```bash
cd /sys/class/pwm/pwmchip0
echo 0 | sudo tee export
echo 1 | sudo tee export
```
_If you look inside the directory using ls, you will now see two new folders: pwm0 and pwm1_

- STEP 2: Configure and Start PWM0 (GPIO 12) with a standard 1 kHz frequency (1,000,000ns period) and a 25% duty cycle.
```bash
cd /sys/class/pwm/pwmchip0/pwm0
echo 1000000 | sudo tee period
echo 250000 | sudo tee duty_cycle
echo 1 | sudo tee enable
```

- STEP 3: Configure and Start PWM1 (GPIO 13) with a standard 1 kHz frequency (1,000,000ns period) and a 25% duty cycle.
```bash
cd /sys/class/pwm/pwmchip0/pwm1
echo 1000000 | sudo tee period
echo 750000 | sudo tee duty_cycle
echo 1 | sudo tee enable
```

- STEP 4: Stop PWM0 and PWM1
```bash
echo 0 | sudo tee /sys/class/pwm/pwmchip0/pwm0/enable
echo 0 | sudo tee /sys/class/pwm/pwmchip0/pwm1/enable
cd /sys/class/pwm/pwmchip0
echo 0 | sudo tee unexport
echo 1 | sudo tee unexport
```