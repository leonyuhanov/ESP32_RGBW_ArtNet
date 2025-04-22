xcopy .\build\bootloader\bootloader.bin .\Firmware\bootloader_0x2000.bin /y
xcopy .\build\storage.bin .\Firmware\storage_0x110000.bin /y
xcopy .\build\partition_table\partition-table.bin .\Firmware\partition-table_0x8000.bin /y
xcopy .\build\RGBArtnetDriverC5Experimental.bin .\Firmware\RGBArtnetDriverC5Experimental_0x10000.bin /y