idf_component_register(SRCS "main.cpp" SRCS "artNetPacket.cpp"  SRCS "NeoViaSPI.cpp" SRCS "protocol_examples_utils.c" INCLUDE_DIRS ".")
spiffs_create_partition_image(storage ../spiffs_image FLASH_IN_PROJECT)
