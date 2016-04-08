import sys
import glob
import serial
import subprocess
import os

def main():
    # Config
    terminalPath = 'putty.exe'

    while True:
        # Rest
        ports = getSerialPorts()
        processes = []

        for i in range(len(ports)):
            command = terminalPath+" -serial "+ports[i]+" -sercfg 38400,8,n,1,R"
            p = subprocess.Popen(command, shell=True)
            processes.append(p)

        print("Opening terminals")
        print(ports)


        wait = raw_input("Press Enter to close all shells")

        subprocess.Popen("TASKKILL /F /IM putty.exe")

        wait = raw_input("Press Enter open them again")

    print("exiting")



def getSerialPorts():
    """ Lists serial port names

        :raises EnvironmentError:
            On unsupported or unknown platforms
        :returns:
            A list of the serial ports available on the system
    """
    if sys.platform.startswith('win'):
        ports = ['COM%s' % (i + 1) for i in range(256)]
    elif sys.platform.startswith('linux') or sys.platform.startswith('cygwin'):
        # this excludes your current terminal "/dev/tty"
        ports = glob.glob('/dev/tty[A-Za-z]*')
    elif sys.platform.startswith('darwin'):
        ports = glob.glob('/dev/tty.*')
    else:
        raise EnvironmentError('Unsupported platform')

    result = []
    for port in ports:
        try:
            s = serial.Serial(port)
            s.close()
            result.append(port)
        except (OSError, serial.SerialException):
            pass
    return result


if __name__ == '__main__':
    main()
