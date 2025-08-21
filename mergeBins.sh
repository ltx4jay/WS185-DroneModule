../esptool-macos-arm64/esptool --chip esp32s3 merge_bin \
  -o DroneModule.bin \
  --flash-mode dio \
  --flash-freq 80m \
  --flash-size 16MB \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0x10000 build/WS185-DroneModule.bin