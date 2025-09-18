# -*- coding: utf-8 -*-

#  MIT License, Copyright (c) 2025 Mitsumine Suzu / verylowfreq
#  Please refer LICENSE file for full text.


import time
import datetime
from typing import Any
import traceback
import io
import base64

# This will be created by the user running the encode_image.py script
import image_data

# requirement: pycaw
from pycaw.pycaw import AudioUtilities, IAudioEndpointVolume
from pycaw.pycaw import AudioUtilities
from comtypes import CLSCTX_ALL, CoInitialize, CoUninitialize

# requirement: psutil
import psutil

# requirement: pystray, Pillow
from PIL import Image, ImageDraw, ImageEnhance
import pystray


import lib_usbclcd


def __get_active_speaker() -> Any:
    devices = AudioUtilities.GetSpeakers()  # スピーカーデバイスの取得
    return devices

def get_master_volume() -> int:
    speaker = __get_active_speaker()
    interface = speaker.Activate(IAudioEndpointVolume._iid_,CLSCTX_ALL, None)  # 動作中のIFを取得
    volume=interface.QueryInterface(IAudioEndpointVolume)
    (vol_min, vol_max, vol_boost) = volume.GetVolumeRange()         # Volのレンジを取得
    vol = volume.GetMasterVolumeLevel()  # 現在Vol 取得
    # NOTE: (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
    return int((vol - vol_min) * (100 - 0) / (vol_max - vol_min) + 0)

def set_master_volume(level:int) -> None:
    # print(level)
    speaker = __get_active_speaker()
    interface = speaker.Activate(IAudioEndpointVolume._iid_,CLSCTX_ALL, None)  # 動作中のIFを取得
    volume=interface.QueryInterface(IAudioEndpointVolume)
    (vol_min, vol_max, vol_boost) = volume.GetVolumeRange()         # Volのレンジを取得
    vol_value = (level - 0) * (vol_max - vol_min) / (100 - 0) + vol_min
    volume.SetMasterVolumeLevel(vol_value, None)

def get_mute() -> bool:
    speaker = __get_active_speaker()
    interface = speaker.Activate(IAudioEndpointVolume._iid_,CLSCTX_ALL, None)  # 動作中のIFを取得
    volume=interface.QueryInterface(IAudioEndpointVolume)
    return volume.GetMute()

def set_mute(muted:bool) -> None:
    speaker = __get_active_speaker()
    interface = speaker.Activate(IAudioEndpointVolume._iid_,CLSCTX_ALL, None)  # 動作中のIFを取得
    volume=interface.QueryInterface(IAudioEndpointVolume)
    volume.SetMute(1 if muted else 0, None)


def pad16(s: str) -> str:
    return (s or "")[:16].ljust(16)


def clcd_worker(icon):
    CoInitialize()
    try:
        # The user manually added this, but with setup(), it's redundant.
        # It is, however, harmless.
        icon.visible = True

        clcd = lib_usbclcd.USBCLCD()

        prev_button = False
        prev_pos = 0
        last_draw = 0.0

        # --- UI Helper Functions ---
        def create_connected_menu(product, serial):
            return pystray.Menu(
                pystray.MenuItem(
                    f"{product}#{serial}",
                    None,
                    enabled=False),
                pystray.Menu.SEPARATOR,
                pystray.MenuItem(
                    'Exit',
                    exit_action
                )
            )

        def set_disconnected_state(icon):
            if icon.icon != icon.image_disconnected:
                icon.icon = icon.image_disconnected
                icon.title = "USB-CLCD (Disconnected)"
                icon.menu = pystray.Menu(pystray.MenuItem('Exit', exit_action))

        def set_connected_state(icon, product, serial):
            icon.icon = icon.image_connected
            icon.title = f"{product}#{serial}"
            icon.menu = create_connected_menu(product, serial)

        # --- Main Logic ---
        def draw(clcd_device):
            try:
                vol = get_master_volume()
                muted = get_mute()
                pct = int(round(vol))
                if muted:
                    line2 = f"Mute   "
                else:
                    line2 = f"Vol {pct:3d}"
                clcd_device.set_cursor(0, 0)
                now = datetime.datetime.now()
                clcd_device.print(now.strftime("%m/%d %a  %H:%M"))
                clcd_device.set_cursor(1, 0)
                clcd_device.print(line2)

                clcd_device.set_cursor(1, 9)
                cpu_percentage = int(psutil.cpu_percent())
                clcd_device.print(f"CPU {cpu_percentage:3d}")
            except Exception as e:
                traceback.print_exc()
                print(f"Error in draw loop: {e}")
                raise lib_usbclcd.USBCLCD.NotRespondError from e

        # Set initial state to disconnected
        set_disconnected_state(icon)

        while icon.visible:
            try:
                if not clcd.is_open():
                    set_disconnected_state(icon) # Ensure UI is disconnected
                    clcd.open()
                    productname, serialstring = clcd.get_product_serial()
                    
                    set_connected_state(icon, productname, serialstring)

                    # Soft blink to show it's alive
                    for _ in range(2):
                        clcd.set_backlight(True); time.sleep(0.15)
                        clcd.set_backlight(False); time.sleep(0.15)
                    clcd.set_backlight(True)
                    clcd.clear()
                    draw(clcd)  # Initial draw

                # --- Main operational loop ---
                button, pos = clcd.get_inputs()

                if button and not prev_button:
                    try:
                        set_mute(not get_mute())
                    except Exception: pass

                if pos != 0:
                    try:
                        current = get_master_volume()
                        step = pos * 4
                        set_master_volume(int(max(0, min(100, current + step))))
                    except Exception: pass

                now = time.time()
                if (button != prev_button) or (pos != prev_pos) or (now - last_draw) > 0.05:
                    draw(clcd)
                    last_draw = now
                
                prev_pos = pos
                prev_button = button

                time.sleep(0.01)

            except lib_usbclcd.USBCLCD.NotFoundError:
                set_disconnected_state(icon)
                time.sleep(1)
                continue
            except (lib_usbclcd.USBCLCD.NotRespondError, lib_usbclcd.USBCLCD.NotConnectedError, BrokenPipeError):
                if clcd.is_open():
                    clcd.close()
                set_disconnected_state(icon)
                time.sleep(1)
                continue
            except Exception:
                traceback.print_exc()
                if clcd.is_open():
                    clcd.close()
                set_disconnected_state(icon)
                time.sleep(5)
                continue

        # --- Cleanup ---
        if clcd.is_open():
            try:
                clcd.set_backlight(False)
                clcd.clear()
            except Exception: pass
            clcd.close()
    finally:
        CoUninitialize()

def exit_action(icon, item):
    icon.visible = False
    icon.stop()

def main() -> None:
    try:
        # Decode the Base64 string and load the image from memory
        image_data_bytes = base64.b64decode(image_data.ICON_DATA)
        image_file = io.BytesIO(image_data_bytes)
        image_connected = Image.open(image_file)
    except Exception as e:
        print(f"Error loading embedded image: {e}. Using fallback icons.")
        image_connected = Image.new('RGB', (64, 64), 'black')
        ImageDraw.Draw(image_connected).text((10, 10), "VOL", fill='white')

    # Create a grayed-out version for the disconnected state by reducing brightness and contrast
    enhancer = ImageEnhance.Brightness(image_connected)
    image_disconnected = enhancer.enhance(0.6) # reduce brightness
    enhancer = ImageEnhance.Contrast(image_disconnected)
    image_disconnected = enhancer.enhance(0.7) # reduce contrast

    # Create the icon, starting with the disconnected image
    icon = pystray.Icon(
        "USBCLCD-Volume",
        image_disconnected,
        "USB-CLCD (Disconnected)",
        menu=pystray.Menu(
            pystray.MenuItem(
                'Exit',
                exit_action
            )
        )
    )
    
    # Store images on the icon object so the worker can access them
    icon.image_connected = image_connected
    icon.image_disconnected = image_disconnected

    icon.run(setup=clcd_worker)


if __name__ == "__main__":
    main()
