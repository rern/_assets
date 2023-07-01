#!/usr/bin/python

import json
import os
import os.path
from camilladsp import CamillaConnection, CamillaError
cdsp = CamillaConnection( '127.0.0.1', 1234 )
cdsp.connect()

is_connected    = cdsp.is_connected()            # True

config          = cdsp.get_config()                       # object - { 'KEY': VALUE }
config_str      = str( config ).replace( 'False', 'false' ).replace( 'True', 'True' ).replace( "'", '"' )
config          = json.loads( config_str )

version         = '.'.join( cdsp.get_version() ) # '1.0.3'
# volume
mute            = cdsp.get_mute                  # False
volume_in       = cdsp.get_capture_signal_rms()  # [ -8.446171, -8.365935 ]
volume_in_peak  = cdsp.get_capture_signal_peak() # [ -16.462996, -16.014568 ]
volume          = cdsp.get_volume()              # 0.0
volume_out      = cdsp.get_capture_signal_rms()  # [ -8.446171, -8.365935 ]
volume_out_peak = cdsp.get_capture_signal_peak() # [ -16.462996, -16.014568 ]
# tone
bass            = config[ 'filters' ][ 'Bass' ][ 'parameters' ][ 'gain' ]
treble          = config[ 'filters' ][ 'Treble' ][ 'parameters' ][ 'gain' ]
# status
state           = cdsp.get_state().name          # 'RUNNING'
capture_rate    = cdsp.get_capture_rate()        # 44100
rate_adjust     = cdsp.get_rate_adjust()         # 0
clipped_samples = cdsp.get_clipped_samples()     # 0
buffer_level    = cdsp.get_buffer_level()        # 2015
# config
config_name     = cdsp.get_config_name()         # '/srv/http/data/camilladsp/configs/NAME.yml'
config_filename = os.path.basename( config_name )
apply_auto      = os.path.exists( '/srv/http/data/camilladsp/applyauto' )

# tabs
devices         = config[ 'devices' ]
filters         = config[ 'filters' ]
mixers          = config[ 'mixers' ]
pipeline        = config[ 'pipeline' ]

exit

cdsp.set_volume( 0.0 )
cdsp.set_mute( False )

# load existing config file
cdsp.set_config_name( file )
cdsp.reload()

# load + validatation user config file
cdsp.read_config_file( file )
cdsp.read_config( 'yaml' )

# save to config file
validate_config( config )
cdsp.set_config( config )
#cdsp.set_config_raw( 'yaml' )

cdsp.disconnect()
