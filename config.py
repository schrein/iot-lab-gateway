
"""
Common configuration for openocd scripts
"""


CONFIG_FILES_PATH = '/home/root/bin'
NODES_CFG   = {
        'gwt': { 'openocd_cfg_file':'fiteco-gwt.cfg',
            'tty':'/dev/ttyFITECO_GWT', 'baudrate':500000},
        'm3': { 'openocd_cfg_file':'fiteco-m3.cfg',
            'tty':'/dev/ttyFITECO_M3',  'baudrate':500000},
        'a8': { 'openocd_cfg_file':'fiteco-a8.cfg',
            'tty':'/dev/ttyFITECO_A8',  'baudrate':500000},
    }

NODES = NODES_CFG.keys()


