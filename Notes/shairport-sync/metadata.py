import dbus
import dbus.mainloop.glib
from gi.repository import GLib
import sys
import time

def init_dbus():
    # Set up the GLib main loop required for handling D-Bus signals asynchronously
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)

    try:
        bus = dbus.SystemBus()

        # Connect to the exact service name revealed by your busctl output
        bus_name = 'org.mpris.MediaPlayer2.ShairportSync'
        object_path = '/org/mpris/MediaPlayer2'

        player_obj = bus.get_object(bus_name, object_path)

        # Access the standard interfaces
        properties_interface = dbus.Interface(player_obj, 'org.freedesktop.DBus.Properties')
        player_interface = dbus.Interface(player_obj, 'org.mpris.MediaPlayer2.Player')

        print("Successfully connected to Shairport Sync over D-Bus!")
        print("-" * 50)

        # 1. Fetch initial values right away
        fetch_current_status(properties_interface)

        # 2. Set up a listener for real-time changes (State, Metadata, Status changes)
        properties_interface.connect_to_signal("PropertiesChanged", on_properties_changed)

        # 3. Start a background timer to track progress (Position ticks up independently)
        GLib.timeout_add(1000, update_progress, player_interface)

    except dbus.exceptions.DBusException as e:
        print(f"Error connecting to D-Bus: {e}")
        print("Is shairport-sync running and playing?")
        sys.exit(1)

def fetch_current_status(props):
    """Gets the initial data state immediately upon script startup."""
    try:
        # Fetch individual MPRIS properties
        state = props.Get('org.mpris.MediaPlayer2.Player', 'PlaybackStatus')
        metadata = props.Get('org.mpris.MediaPlayer2.Player', 'Metadata')

        print_metadata(metadata)
        print(f"Current State/Status: {state}")
        print("-" * 50)
    except Exception as e:
        print(f"Could not read initial state: {e}")

def print_metadata(metadata):
    """Parses and neatly displays track metadata tags."""
    if not metadata:
        print("Metadata: [No Track Loaded / Idle]")
        return

    title = metadata.get('xesam:title', '[Unknown Title]')
    # MPRIS standard forces artists and albums to be arrays or wrapped objects
    artist_list = metadata.get('xesam:artist', ['[Unknown Artist]'])
    artist = artist_list[0] if artist_list else '[Unknown Artist]'
    album = metadata.get('xesam:album', '[Unknown Album]')

    # Track Length is in MICROSECONDS in MPRIS
    duration_us = metadata.get('mpris:length', 0)
    duration_secs = int(duration_us / 1000000)
    mins, secs = divmod(duration_secs, 60)

    print("\n--- NOW PLAYING ---")
    print(f"Title:    {title}")
    print(f"Artist:   {artist}")
    print(f"Album:    {album}")
    print(f"Duration: {mins:02d}:{secs:02d}")

def update_progress(player_interface):
    """Fires every 1 second to fetch and display the live stream progress."""
    try:
        # Position is a dynamic property, retrieved in microseconds
        # Note: Shairport Sync only increments this if active audio is processing
        position_us = player_interface.Get('org.mpris.MediaPlayer2.Player', 'Position')
        pos_secs = int(position_us / 1000000)
        mins, secs = divmod(pos_secs, 60)

        # Flush line stdout trick to keep the progress counter updating cleanly on one line
        sys.stdout.write(f"\rTrack Progress: {mins:02d}:{secs:02d} ")
        sys.stdout.flush()
    except Exception:
        # If the track is stopped or connection drops, hide progress reporting gracefully
        sys.stdout.write("\rTrack Progress: --:-- (Stopped/Idle) ")
        sys.stdout.flush()

    return True # Keeps the GLib timeout loop ticking

def on_properties_changed(interface_name, changed_properties, invalidated_properties):
    """Triggers instantly whenever a user skips tracks, pauses, or disconnects."""
    if interface_name == 'org.mpris.MediaPlayer2.Player':
        print("\n") # Break the line from the progress counter

        # Catch state/status changes (Playing -> Paused -> Stopped)
        if 'PlaybackStatus' in changed_properties:
            print(f">> State/Status Changed: {changed_properties['PlaybackStatus']}")
            if changed_properties['PlaybackStatus'] in ['Paused', 'Stopped']:
                print(">> Session clean-up hook triggered.")

        # Catch track identity updates
        if 'Metadata' in changed_properties:
            print_metadata(changed_properties['Metadata'])
            print("-" * 50)

if __name__ == '__main__':
    init_dbus()

    # Run the GLib loop indefinitely to catch async network events
    loop = GLib.MainLoop()
    try:
        loop.run()
    except KeyboardInterrupt:
        print("\nExiting monitor...")
        