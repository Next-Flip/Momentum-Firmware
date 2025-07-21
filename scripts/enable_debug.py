import serial.tools.list_ports as list_ports
import serial


def main():
    try:
        flippers = list(list_ports.grep("flip_"))
        if len(flippers) != 1:
            return

        port = serial.Serial(flippers[0].device, 230400)
        port.timeout = 2
    except Exception:
        return

    try:
        port.read_until(b">: ")
        for cmd in (
            b"sysctl debug 1\r",
            b"sysctl sleep_mode legacy\r",
            b"sysctl log_level debug\r",
        ):
            port.write(cmd)
            port.read_until(b">: ")
    except Exception:
        pass
    finally:
        try:
            port.close()
        except Exception:
            pass


if __name__ == "__main__":
    main()
