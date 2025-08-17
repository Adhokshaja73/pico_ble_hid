set(PROJECT ble_hid)
cmake_minimum_required(VERSION 3.13)
set(PICO_SDK_PATH /home/kenji/programming/pico/c/pico-sdk)
set(PICO_PIO_USB_PATH /home/kenji/programming/pico/c/Pico-PIO-USB)
set(PICO_BOARD pico_w)
include (${PICO_SDK_PATH}/external/pico_sdk_import.cmake)
project(${PROJECT} C CXX ASM)

pico_sdk_init()

add_subdirectory(${PICO_PIO_USB_PATH} pico_pio_usb)

add_executable(${PROJECT})
target_sources(${PROJECT} PRIVATE
	bt_device.c
	hid_report.c
	main.c
	#usb_descriptors.c
	usb_device.c
	usb_host.c
)

 # print memory usage, enable all warnings
target_link_options(${PROJECT} PRIVATE -Xlinker --print-memory-usage)
target_compile_options(${PROJECT} PRIVATE ) #-Wall -Wextra

# use tinyusb pio
target_compile_definitions(${PROJECT} PRIVATE PIO_USB_USE_TINYUSB)

# needed so tinyusb can find tusb_config.h
target_include_directories(${PROJECT} PRIVATE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(${PROJECT} PRIVATE
	pico_btstack_ble
	pico_btstack_cyw43
	pico_cyw43_arch_none
	pico_multicore
	pico_pio_usb
	pico_stdlib
	tinyusb_board
	#tinyusb_device
	tinyusb_host
	tinyusb_pico_pio_usb
)

pico_btstack_make_gatt_header(ble_hid PRIVATE "${CMAKE_CURRENT_LIST_DIR}/ble_hid.gatt")

pico_add_extra_outputs(${PROJECT})

