add_executable(tuya-open-sdk
        main.c
        )

# pull in common dependencies
target_link_libraries(tuya-open-sdk pico_stdlib)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(tuya-open-sdk)

# add url via pico_set_program_url
example_auto_set_url(tuya-open-sdk)
