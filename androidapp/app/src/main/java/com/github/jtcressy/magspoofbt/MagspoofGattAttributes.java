package com.github.jtcressy.magspoofbt;

import java.util.HashMap;

public class MagspoofGattAttributes {
    private static HashMap<String, String> attributes = new HashMap();
    public static String TEST_LED_CHARACTERISTIC_UUID = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";
    public static String MAGSPOOF_CHARACTERISTIC_UUID = "d9c28aa1-363b-4c5a-9be9-e41f7937786f";

    static {
        // Sample Services.
        attributes.put("d9c28aa0-363b-4c5a-9be9-e41f7937786f", "MagSpoof Service");
        // Sample Characteristics.
        attributes.put(TEST_LED_CHARACTERISTIC_UUID, "MagSpoof LED Test");
        attributes.put(MAGSPOOF_CHARACTERISTIC_UUID, "MagSpoof Writable Destination");
        attributes.put("00002a29-0000-1000-8000-00805f9b34fb", "Manufacturer Name String");
    }

    public static String lookup(String uuid, String defaultName) {
        String name = attributes.get(uuid);
        return name == null ? defaultName : name;
    }
}
