/* mbed Microcontroller Library
 * Copyright (c) 2006-2020 ARM Limited
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* MBED_DEPRECATED */
#warning "These services are deprecated and will be removed. Please see services.md for details about replacement services."


#include "ble/BLE.h"
#include "ble/Gap.h"
#include "ble/GattServer.h"

#include "stm32l475e_iot01_tsensor.h"
#include "stm32l475e_iot01_hsensor.h"
#include "stm32l475e_iot01_psensor.h"
#include "stm32l475e_iot01_magneto.h"
#include "stm32l475e_iot01_gyro.h"
#include "stm32l475e_iot01_accelero.h"

#if BLE_FEATURE_GATT_SERVER

/**
 * BLE Heart Rate Service.
 *
 * @par purpose
 *
 * Fitness applications use the heart rate service to expose the heart
 * beat per minute measured by a heart rate sensor.
 *
 * Clients can read the intended location of the sensor and the last heart rate
 * value measured. Additionally, clients can subscribe to server initiated
 * updates of the heart rate value measured by the sensor. The service delivers
 * these updates to the subscribed client in a notification packet.
 *
 * The subscription mechanism is useful to save power; it avoids unecessary data
 * traffic between the client and the server, which may be induced by polling the
 * value of the heart rate measurement characteristic.
 *
 * @par usage
 *
 * When this class is instantiated, it adds a heart rate service in the GattServer.
 * The service contains the location of the sensor and the initial value measured
 * by the sensor.
 *
 * Application code can invoke updateHeartRate() when a new heart rate measurement
 * is acquired; this function updates the value of the heart rate measurement
 * characteristic and notifies the new value to subscribed clients.
 *
 * @note You can find specification of the heart rate service here:
 * https://www.bluetooth.com/specifications/gatt
 *
 * @attention The service does not expose information related to the sensor
 * contact, the accumulated energy expanded or the interbeat intervals.
 *
 * @attention The heart rate profile limits the number of instantiations of the
 * heart rate services to one.
 */
class MagService {
public:
    /**
     * Construct and initialize a heart rate service.
     *
     * The construction process adds a GATT heart rate service in @p _ble
     * GattServer, sets the value of the heart rate measurement characteristic
     * to @p hrmCounter and the value of the body sensor location characteristic
     * to @p location.
     *
     * @param[in] _ble BLE device that hosts the heart rate service.
     * @param[in] hrmCounter Heart beats per minute measured by the heart rate
     * sensor.
     * @param[in] location Intended location of the heart rate sensor.
     */
    MagService(BLE &_ble, uint16_t hrmCounter) :
        ble(_ble),
        valueBytes(hrmCounter),
        MagX(
            0xfff0,
            valueBytes.getPointer(),
            valueBytes.getNumValueBytes(),
            MagValueBytes::MAX_VALUE_BYTES,
            GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY
        ),
        MagY(
            0xfff1,
            valueBytes.getPointer(),
            valueBytes.getNumValueBytes(),
            MagValueBytes::MAX_VALUE_BYTES,
            GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY
        ),
        MagZ(
            0xfff2,
            valueBytes.getPointer(),
            valueBytes.getNumValueBytes(),
            MagValueBytes::MAX_VALUE_BYTES,
            GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY
        )
    {
        setupService();
        BSP_MAGNETO_Init();
    }

    /**
     * Update the heart rate that the service exposes.
     *
     * The server sends a notification of the new value to clients that have
     * subscribed to updates of the heart rate measurement characteristic; clients
     * reading the heart rate measurement characteristic after the update obtain
     * the updated value.
     *
     * @param[in] hrmCounter Heart rate measured in BPM.
     *
     * @attention This function must be called in the execution context of the
     * BLE stack.
     */
    void updateMag() {
        BSP_MAGNETO_GetXYZ(pDataXYZ);
        printf("x=%d\n",pDataXYZ[0]);
        printf("y=%d\n",pDataXYZ[1]);
        printf("z=%d\n",pDataXYZ[2]);
        valueBytes.updateMag(pDataXYZ[0]);
        ble.gattServer().write(
            MagX.getValueHandle(),
            valueBytes.getPointer(),
            valueBytes.getNumValueBytes()
        );
        valueBytes.updateMag(pDataXYZ[1]);
        ble.gattServer().write(
            MagY.getValueHandle(),
            valueBytes.getPointer(),
            valueBytes.getNumValueBytes()
        );
        valueBytes.updateMag(pDataXYZ[2]);
        ble.gattServer().write(
            MagZ.getValueHandle(),
            valueBytes.getPointer(),
            valueBytes.getNumValueBytes()
        );
    }

protected:
    /**
     * Construct and add to the GattServer the heart rate service.
     */
    void setupService() {
        GattCharacteristic *charTable[] = {
            &MagX,
            &MagY,
            &MagZ
        };
        GattService MagService(
            0x2AA1,
            charTable,
            sizeof(charTable) / sizeof(charTable[0])
        );

        ble.gattServer().addService(MagService);
    }

protected:
    /*
     * Heart rate measurement value.
     */
    struct MagValueBytes {
        /* 1 byte for the Flags, and up to two bytes for heart rate value. */
        static const unsigned MAX_VALUE_BYTES = 3;
        static const unsigned FLAGS_BYTE_INDEX = 0;

        static const unsigned VALUE_FORMAT_BITNUM = 0;
        static const uint8_t  VALUE_FORMAT_FLAG = (1 << VALUE_FORMAT_BITNUM);

        MagValueBytes(uint16_t hrmCounter) : valueBytes()
        {
            updateMag(hrmCounter);
        }

        void updateMag(uint16_t hrmCounter)
        {
            if (hrmCounter <= 255) {
                valueBytes[FLAGS_BYTE_INDEX] &= ~VALUE_FORMAT_FLAG;
                valueBytes[FLAGS_BYTE_INDEX + 1] = hrmCounter;
            } else {
                valueBytes[FLAGS_BYTE_INDEX] |= VALUE_FORMAT_FLAG;
                valueBytes[FLAGS_BYTE_INDEX + 2] = (uint8_t)(hrmCounter & 0xFF);
                valueBytes[FLAGS_BYTE_INDEX + 1] = (uint8_t)(hrmCounter >> 8);
            }
        }

        uint8_t *getPointer()
        {
            return valueBytes;
        }

        const uint8_t *getPointer() const
        {
            return valueBytes;
        }

        unsigned getNumValueBytes() const
        {
            if (valueBytes[FLAGS_BYTE_INDEX] & VALUE_FORMAT_FLAG) {
                return 1 + sizeof(uint16_t);
            } else {
                return 1 + sizeof(uint8_t);
            }
        }

    private:
        uint8_t valueBytes[MAX_VALUE_BYTES];
    };
    
protected:
    BLE &ble;
    MagValueBytes valueBytes;
    GattCharacteristic MagX;
    GattCharacteristic MagY;
    GattCharacteristic MagZ;
    int16_t pDataXYZ[3] = {0};
};

#endif // BLE_FEATURE_GATT_SERVER

