import io
import gzip
import tarfile

import heatshrink2

from .heatshrink_stream import HeatshrinkDataStreamHeader

FLIPPER_TAR_FORMAT = tarfile.USTAR_FORMAT

TAR_HEATSHRINK_EXTENSION = ".ths"
TAR_GZIP_EXTENSION = ".tar.gz"


def tar_sanitizer_filter(tarinfo: tarfile.TarInfo):
    tarinfo.gid = tarinfo.uid = 0
    tarinfo.mtime = 0
    tarinfo.uname = tarinfo.gname = "furippa"
    if tarinfo.type == tarfile.DIRTYPE:
        tarinfo.mode = 0o40755  # drwxr-xr-x
    else:
        tarinfo.mode = 0o644  # ?rw-r--r--
    return tarinfo


def compress_tree_tarball(
    src_dir,
    output_name,
    filter=tar_sanitizer_filter,
    hs_window=13,
    hs_lookahead=6,
    gz_level=9,
):
    plain_tar = io.BytesIO()
    with tarfile.open(
        fileobj=plain_tar,
        mode="w:",
        format=FLIPPER_TAR_FORMAT,
    ) as tarball:
        tarball.add(src_dir, arcname="", filter=filter)
    plain_tar.seek(0)
    src_data = plain_tar.read()

    if output_name.endswith(TAR_HEATSHRINK_EXTENSION):
        compressed = heatshrink2.compress(
            src_data, window_sz2=hs_window, lookahead_sz2=hs_lookahead
        )
        header = HeatshrinkDataStreamHeader(hs_window, hs_lookahead)
        compressed = header.pack() + compressed

    elif output_name.endswith(TAR_GZIP_EXTENSION):
        compressed = gzip.compress(src_data, compresslevel=gz_level, mtime=0)

    else:
        compressed = src_data

    with open(output_name, "wb") as f:
        f.write(compressed)
    return len(src_data), len(compressed)
