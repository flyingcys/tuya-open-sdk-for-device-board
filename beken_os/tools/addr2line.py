import argparse
import os

# input_filename = 'tuyaos_iot_rtl8720c_350_5.0.0_DEBUG.asm'
# backtrace = '9b008716 9b00776a 9b007872 9b00baac 9b00baa0 9b024394 9b0625fe 9b05f9ae 9b04c854 9b04e5aa 9b04c7e8 9b04dee6 9b009c54'

parser = argparse.ArgumentParser(description='Checksum for file')
parser.add_argument('--file', '-f', help='file path,必要参数', required=True)
parser.add_argument('--backtrace', '-b', help='backtrace,必要参数,', required=True)
args = parser.parse_args()


def addr2line(input_filename, backtrace):
    backtrace_list = backtrace.split(' ')
    fp = open(input_filename, 'r')
    if fp:
        for bt in backtrace_list:
            fp.seek(0)
            func_line = ''
            for line in fp:
                if '>:' in line:
                    func_line = line
                if bt+':' in line:
                    # print(func_line)
                    func_name = func_line[func_line.find(
                        '<')+1:func_line.find('>')]
                    print(func_name)
                    break
        fp.close()


if __name__ == '__main__':
    try:
        addr2line(args.file, args.backtrace)
    except Exception as e:
        print(e)

# python ../../../../vendor/bk7231n/bk7231n_os/tools/addr2line.py -f tuyaos_iot_bk7231n_common_app_350_0.0.5.asm -b '498a8 49be8 49dec 1968e fa59c 4b098'
