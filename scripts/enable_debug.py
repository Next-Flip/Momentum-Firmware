import serial.tools.list_ports as list_ports
import serial


def main():
    try:
        flippers = list(list_ports.grep("flip_"))
        if len(flippers) != 1:
            return

        port = serial.Serial()
        port.port = flippers[0].device
        port.timeout = 0.1
        port.write_timeout = 0.1
        port.baudrate = 115200  # Doesn't matter for VCP
        port.open()
    except Exception:
        return

    try:
        port.write(
            b"sysctl debug 1\r"
            b"sysctl sleep_mode legacy\r"
            b"sysctl log_level debug\r"
        )
    except Exception:
        pass
    finally:
        try:
            port.close()
        except Exception:
            pass


if __name__ == "__main__":
    main()
