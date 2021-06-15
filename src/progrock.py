import argparse
from asyncio import get_event_loop
from math import ceil
import os
from sys import stdout
from serial_asyncio import open_serial_connection
from serial.serialutil import SerialException
import time


CHUNK_SIZE = 1024
bytes_ok = bytes('OK\r\n', 'ascii')
bytes_err = bytes('ERROR\r\n', 'ascii')


class ProtocolError (Exception):
    pass


async def run(args):
    try:
        reader, writer = await open_serial_connection(url=args.tty, baudrate=args.baud, timeout=3)
        time.sleep(2)
    except SerialException as e:
        print(f"Unable to open a serial connection to {args.tty}")
        print('\t' + e)
        exit(1)

    if args.image and not args.len:
        with open(args.image, 'rb') as img:
            img.seek(0, os.SEEK_END)
            file_len = img.tell()
            img.seek(0)

            print(f"Image: {args.image}  Start: {args.start}  Length: {file_len}")
            print("Uploading", end=' ', flush=True)

            for offset in range(0, file_len, CHUNK_SIZE):
                chunk_bytes = img.read(CHUNK_SIZE)
                chunk_len = len(chunk_bytes)
                writer.write(f'w {args.start + offset} {chunk_len}\n'.encode('ascii'))

                if await reader.readline() == bytes_ok:
                    writer.write(chunk_bytes)

                    if await reader.readline() != bytes_ok:
                        print('')
                        raise ProtocolError('Programmer did not acknowledge successful transfer')

                    print('.', end='', flush=True)
                else:
                    print('')
                    raise ProtocolError('Programmer did not respond to write command')

                writer.write(b'\n')

            print(" done")
    elif args.len and not args.image:
        for offset in range(0, args.len, CHUNK_SIZE):
            start = args.start + offset
            length = min(CHUNK_SIZE, args.len - offset)

            writer.write(f'r {start} {length}\n'.encode('ascii'))
            response = await reader.readline()
            if (response == bytes_ok):
                data = b''

                while len(data) < length:
                    data += await reader.read(length - len(data))

                if args.canonical:
                    for index in range(0, len(data)):
                        if index % 16 == 0:
                            print(f'{index + start:08x}', end='  ')
                        if index % 16 == 8:
                            print('', end=' ')
                        print(f'{data[index]:02x}', end=' ')
                        if index % 16 == 15:
                            print('')
                else:
                    with os.fdopen(stdout.fileno(), 'wb', closefd=False) as out:
                        out.write(data)
                        out.flush()

                if (await reader.readline() != bytes_ok):
                    raise ProtocolError('Programmer did not transmit end of data marker')
            else:
                raise ProtocolError(str(response, 'utf-8').strip())

            writer.write(b'\n')

        if args.canonical:
            print('')
    else:
        raise NotImplementedError('Unsupported parameters')


def main():
    # Parse arguments
    parser = argparse.ArgumentParser()
    parser.add_argument('--tty', type=str, default=None,
            help="the serial interface to connect via")
    parser.add_argument('--baud', type=int, default=115200,
            help="the baud rate of the connection")
    parser.add_argument('--start', type=int, default=0, required=True,
            help="the starting address of the read or write operation")
    parser.add_argument('--image', type=str, default=None,
            help="the image to load")
    parser.add_argument('--len', type=int, default=None,
            help="the number of bytes to read")
    parser.add_argument('-C', action='store_true', dest='canonical',
            help="""Display the input offset in hexadecimal, followed by
                    sixteen space-separated, two column, hexadecimal bytes""")

    args = parser.parse_args()
    args.tty = args.tty or os.environ.get('PROGROCK_TTY', None)

    if not args.tty:
        raise EnvironmentError('missing requred argument --tty ' +
                'or environment var PROGROCK_TTY')

    loop = get_event_loop()
    loop.run_until_complete(run(args))


if __name__ == "__main__":
    main()

