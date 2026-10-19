# Ceiling for the stripped size of the size canary, in bytes, per platform. Raise it deliberately, in
# the commit that adds the code, and say why. 0 = report only (nothing to protect yet).
set(SPUX_CANARY_MAX_BYTES_LINUX 0)
set(SPUX_CANARY_MAX_BYTES_WINDOWS 0)
set(SPUX_CANARY_MAX_BYTES_MACOS 0)
