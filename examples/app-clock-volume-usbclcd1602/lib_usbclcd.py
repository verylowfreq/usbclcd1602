# -*- coding: utf-8 -*-

#  MIT License, Copyright (c) 2025 Mitsumine Suzu / verylowfreq
#  Please refer LICENSE file for full text.

# パソコン上のPython向けのUSB-CLCD1602ライブラリです

# Requirements: hidapi
import hid

from typing import Optional, Tuple, Iterable
import struct
import time


class USBCLCD:
    """
    USB接続キャラクター液晶(USB-CLCD1602)を制御するためのライブラリ。
    
    HIDデバイスとして通信し、テキスト表示、バックライト制御、
    およびロータリーエンコーダーとボタン入力の取得機能を提供します。

    キャラクタ液晶は16文字2行、ASCII文字と半角カタカナが表示できます。
    """

    # --- 例外クラス ---
    class NotFoundError(Exception):
        """デバイスが見つからない場合のエラー。"""
        pass

    class NotConnectedError(Exception):
        """デバイスに接続されていない場合のエラー。"""
        pass

    class NotRespondError(Exception):
        """デバイスが応答しない場合のエラー。"""
        pass

    # --- デバイス定数 ---
    DEVICE_VID = 0xf055  # ベンダーID
    DEVICE_PID = 0x6584  # プロダクトID
    DEVICE_PRODUCTNAME = "USB-CLCD1602"  # プロダクト名

    # --- CLCD仕様 ---
    CLCD_ROW = 2  # 液晶の行数
    CLCD_COL = 16  # 液晶の桁数

    DEFAULT_TIMEOUT = 30

    def __init__(self) -> None:
        """コンストラクタ。内部変数を初期化します。"""
        self.vid = -1
        self.pid = -1
        self.productname = ""
        self.serialnumber = None
        self.hid = None
        # get_inputs()が失敗した際に返すための前回の入力値
        self.prev_button = False
        self.prev_val = 0


    def open(self, *, serialnumber:Optional[str]=None) -> None:
        """
        デバイスに接続します。
        
        VID, PIDに一致するデバイスに接続し、プロダクト名が一致するか検証します。
        シリアル番号を指定すると、複数のデバイスの中から特定の1台に接続できます。

        Args:
            serialnumber (Optional[str], optional): 接続するデバイスのシリアル番号。

        Raises:
            USBCLCD.NotFoundError: デバイスが見つからないか、プロダクト名が一致しない場合。
        """
        self.vid = self.DEVICE_VID
        self.pid = self.DEVICE_PID
        self.productname = self.DEVICE_PRODUCTNAME
        self.serialnumber = serialnumber

        self.prev_button = False
        self.prev_val = 0                    

        self.hid = hid.device()
        try:
            self.hid.open(self.vid, self.pid, self.serialnumber)
        except OSError:
            raise USBCLCD.NotFoundError()
        
        # 接続したデバイスのプロダクト名が期待値と一致するか確認
        connected_productname = self.hid.get_product_string()
        if connected_productname != self.productname:
            self.hid.close()
            self.hid = None
            raise USBCLCD.NotFoundError()
        
        self.serialstring = self.hid.get_serial_number_string()


    def close(self) -> None:
        """デバイスとの接続を閉じます。"""
        if self.hid is not None:
            self.hid.close()
            self.hid = None

    def is_open(self) -> bool:
        """デバイスがオープンされているかを確認します。"""
        return self.hid is not None

    
    def get_product_serial(self) -> Tuple[str, str]:
        """
        接続しているデバイスのプロダクト名とシリアル番号を取得します。

        Raises:
            USBCLCD.NotConnectedError: デバイスに接続されていない場合。

        Returns:
            Tuple[str, str]: (プロダクト名, シリアル番号)のタプル。
        """
        if self.hid is None:
            raise USBCLCD.NotConnectedError()
        return (self.productname, self.serialstring)
    
    def __send(self, buffer:bytes) -> None:
        if not self.is_open():
            raise USBCLCD.NotConnectedError()
        assert(self.hid is not None)
        buf = bytes([0]) + buffer
        try:
            self.hid.write(buf)
        except ValueError:
            self.close()
            raise USBCLCD.NotConnectedError()
        except OSError:
            raise USBCLCD.NotRespondError()

    def __read(self, readlen:int) -> bytes:
        if not self.is_open():
            raise USBCLCD.NotConnectedError()
        assert(self.hid is not None)
        try:
            data = self.hid.read(readlen, self.DEFAULT_TIMEOUT)
        except ValueError:
            self.close()
            raise USBCLCD.NotConnectedError()
        except OSError:
            raise USBCLCD.NotRespondError()
        
        if data is None:
            return bytes([])
        else:
            return data

    def send_command(self, command: int) -> None:
        """
        液晶コントローラ(HD44780互換)にコマンドを送信します。

        Args:
            command (int): 送信するコマンドコード。

        Raises:
            USBCLCD.NotConnectedError: デバイスに接続されていない場合。
            USBCLCD.NotRespondError: デバイスが応答しない場合。
        """
        if self.hid is None:
            raise USBCLCD.NotConnectedError()
        # HIDレポートのバッファを作成
        buf = [0] * 64
        buf[0] = 0x02  # レポートID or コマンド種別: 汎用コマンド送信
        buf[1] = command
        self.__send(bytes(buf))
    

    def print(self, text:str) -> None:
        """
        現在のカーソル位置に文字列を表示します。
        転送データは最大32文字まで。超えた分は切り捨てられます。

        Args:
            text (str): 表示する文字列。

        Raises:
            USBCLCD.NotConnectedError: デバイスに接続されていない場合。
            USBCLCD.NotRespondError: デバイスが応答しない場合。
        """
        if self.hid is None:
            raise USBCLCD.NotConnectedError()
        text = text[:32]
        buf = [0] * 64
        # 1文字ずつHIDレポートに詰める
        for i in range(len(text)):
            buf[i * 2 + 0] = 0x01  # レポートID or コマンド種別: 文字データ送信
            buf[i * 2 + 1] = ord(text[i])
        self.__send(bytes(buf))
    

    def set_cursor(self, row:int, col:int) -> None:
        """
        カーソル位置を設定します。

        Args:
            row (int): 行番号 (0 or 1)
            col (int): 桁番号 (0-15)
        """
        if self.hid is None:
            raise USBCLCD.NotConnectedError()
        row = row % self.CLCD_ROW
        col = col % self.CLCD_COL
        # HD44780のDDRAMアドレス計算
        command = 0x80 + 0x40 * row + col
        self.send_command(command)


    def set_backlight(self, enabled:bool) -> None:
        """
        バックライトのON/OFFを設定します。

        Args:
            enabled (bool): TrueでON, FalseでOFF。
        """
        if self.hid is None:
            raise USBCLCD.NotConnectedError()
        buf = [0] * 64
        buf[0] = 0x03  # レポートID or コマンド種別: バックライト制御
        buf[1] = 0x01 if enabled else 0x00
        self.__send(bytes(buf))


    def clear(self) -> None:
        """画面をクリアし、カーソルをホームポジション(0, 0)に移動します。"""
        command = 0x01  # HD44780の"Clear Display"コマンド
        self.send_command(command)
        time.sleep(0.01)  # コマンド実行のためのウェイト


    def get_inputs(self) -> Tuple[bool, int]:
        """
        ロータリーエンコーダーとボタンの状態を取得します。

        Raises:
            USBCLCD.NotConnectedError: デバイスに接続されていない場合。
            USBCLCD.NotRespondError: デバイスが応答しない場合。

        Returns:
            Tuple[bool, int]: (ボタン状態, エンコーダー差分値)
                              ボタン状態: True=押されている, False=押されていない
                              エンコーダー差分値: 前回取得時からの回転量
        """
        buf = self.__read(8)
        
        # 読み取り失敗時は前回の値を返す
        if buf is None or len(buf) != 8:
            return (self.prev_button, 0) # 差分なので0を返す
        
        # 入力レポートのデコード
        button = (buf[0] != False)  # 1バイト目: ボタン状態
        val = struct.unpack("<b", bytes([ buf[1] ]))[0]  # 2バイト目: エンコーダー差分値 (符号付き8bit整数)
        
        self.prev_button = button
        self.prev_val = val
        return (button, val)


    def reset_bootloader(self) -> None:
        """
        デバイスをブートローダーモードに移行させます。
        ファームウェアの更新などに使用します。
        """
        if self.hid is None:
            raise USBCLCD.NotConnectedError()
        buf = [0] * 64
        buf[0] = 0xaa  # ブートローダー移行用のマジックコマンド
        buf[1] = 0xff
        try:
            self.__send(bytes(buf))
        except OSError:
            # ブートローダーに移行するとデバイスが応答しなくなるため、
            # NotRespondErrorは発生するが、ここでは無視してよい
            pass
