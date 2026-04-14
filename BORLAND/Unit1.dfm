object Form1: TForm1
  Left = 891
  Top = 280
  Width = 401
  Height = 565
  Caption = 'ESP32_CALC_PAINT'
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  OldCreateOrder = False
  OnCreate = FormCreate
  PixelsPerInch = 96
  TextHeight = 13
  object GB_ESP_CONNECTION: TGroupBox
    Left = 24
    Top = 16
    Width = 337
    Height = 161
    Caption = 'ESP CONNECTION TO PC'
    TabOrder = 0
    object LBL_CONNECTION_STATUS: TLabel
      Left = 24
      Top = 24
      Width = 139
      Height = 13
      Caption = 'CONNECTION STATUS:'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -11
      Font.Name = 'MS Sans Serif'
      Font.Style = [fsBold]
      ParentFont = False
    end
    object LBL_CHOOSE_COM_PORT: TLabel
      Left = 24
      Top = 48
      Width = 91
      Height = 13
      Caption = 'Choose COM Port :'
    end
    object CMB_COM_PORT: TComboBox
      Left = 24
      Top = 72
      Width = 145
      Height = 21
      ItemHeight = 13
      TabOrder = 0
    end
    object BTN_CONNECT: TButton
      Left = 200
      Top = 48
      Width = 81
      Height = 25
      Caption = 'CONNECT'
      TabOrder = 1
      OnClick = BTN_CONNECTClick
    end
    object BTN_DISCONNECT: TButton
      Left = 200
      Top = 80
      Width = 81
      Height = 25
      Caption = 'DISCONNECT'
      TabOrder = 2
      OnClick = BTN_DISCONNECTClick
    end
  end
  object GB_ESP_INVOKE_CALC_PAINT: TGroupBox
    Left = 24
    Top = 200
    Width = 337
    Height = 97
    Caption = 'ESP INVOKE CALC OR PAINT'
    TabOrder = 1
    object LBL_CALC_STATUS: TLabel
      Left = 16
      Top = 24
      Width = 216
      Height = 13
      Caption = 'INVOKE CALC. PRESS D4. STATUS :'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -11
      Font.Name = 'MS Sans Serif'
      Font.Style = [fsBold]
      ParentFont = False
    end
    object LBL_PAINT_STATUS: TLabel
      Left = 16
      Top = 56
      Width = 229
      Height = 13
      Caption = 'INVOKE PAINT. PRESS D16. STATUS :'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -11
      Font.Name = 'MS Sans Serif'
      Font.Style = [fsBold]
      ParentFont = False
    end
  end
  object GB_ESP_INVOKE_ANY_PROGRAM: TGroupBox
    Left = 24
    Top = 320
    Width = 337
    Height = 161
    Caption = 'ESP INVOKE ANY WINDOWS PROGRAM'
    TabOrder = 2
    object LBL_D4_PINNED_TO: TLabel
      Left = 8
      Top = 56
      Width = 97
      Height = 13
      Caption = 'D4 PINNED TO :'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -11
      Font.Name = 'MS Sans Serif'
      Font.Style = [fsBold]
      ParentFont = False
    end
    object LBL_D16_PINNED_TO: TLabel
      Left = 8
      Top = 119
      Width = 104
      Height = 13
      Caption = 'D16 PINNED TO :'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -11
      Font.Name = 'MS Sans Serif'
      Font.Style = [fsBold]
      ParentFont = False
    end
    object BTN_PIN_TO_D4: TButton
      Left = 8
      Top = 24
      Width = 75
      Height = 25
      Caption = 'PIN TO D4'
      TabOrder = 0
      OnClick = BTN_PIN_TO_D4Click
    end
    object BTN_PIN_TO_D16: TButton
      Left = 8
      Top = 87
      Width = 75
      Height = 25
      Caption = 'PIN TO D16'
      TabOrder = 1
      OnClick = BTN_PIN_TO_D16Click
    end
  end
  object SB_MAIN_STATUS_BAR: TStatusBar
    Left = 0
    Top = 507
    Width = 385
    Height = 19
    Panels = <>
    SimplePanel = False
  end
  object TimerReadCom: TTimer
    Enabled = False
    Interval = 50
    OnTimer = TimerReadComTimer
    Left = 352
    Top = 472
  end
end
