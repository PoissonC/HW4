# ble_scan_connect.py:
from bluepy.btle import Peripheral, UUID
from bluepy.btle import Scanner, DefaultDelegate
import struct
class ScanDelegate(DefaultDelegate):
        def __init__(self):
                DefaultDelegate.__init__(self)
        def handleDiscovery(self, dev, isNewDev, isNewData):
            if isNewDev:
               print ("Discovered device", dev.addr)
            elif isNewData:
                print ("Received new data from", dev.addr)
        def handleNotification(self, cHandle, data):
            print ('Data received')
#            print (cHandle)    #use this to check the  handle value of each characteristic
            if cHandle < 19:
                print ("HeartRate is %d bpm" %data[1])
            else:
                if data[0]:
                    k=struct.unpack('!Bh',data)
                    out=k[1]
                else:
                    out=int(data[1])
                if cHandle < 22:
                    print ("MagnoMeter_x = %d"%out)
                elif cHandle < 25:
                    print ("MagnoMeter_y = %d"%out)
                else:
                    print ("MagnoMeter_z = %d"%out)
scanner = Scanner().withDelegate(ScanDelegate())
devices = scanner.scan(3.0)

print ("Connecting...")
dev = Peripheral('e1:b2:60:72:64:aa', 'random')
dev.setDelegate(ScanDelegate())
#
print ("Services...")
for svc in dev.services:
        print (str(svc))
#
try:
    MagnoService = dev.getServiceByUUID(UUID(0x2AA1))
    HeartRateService = dev.getServiceByUUID(UUID(6157))
    HeartRate = HeartRateService.getCharacteristics(UUID(10807))
    magX = MagnoService.getCharacteristics(UUID(0xfff0))
    magY = MagnoService.getCharacteristics(UUID(0xfff1))
    magZ = MagnoService.getCharacteristics(UUID(0xfff2))
# start notification
    dev.writeCharacteristic(HeartRate[0].valHandle + 1, b'\x01\x00')
    dev.writeCharacteristic(magX[0].valHandle + 1, b'\x01\x00')
    dev.writeCharacteristic(magY[0].valHandle + 1, b'\x01\x00')
    dev.writeCharacteristic(magZ[0].valHandle + 1, b'\x01\x00')
    while True:
        if dev.waitForNotifications(1.0):
            continue
finally:
    dev.disconnect()
