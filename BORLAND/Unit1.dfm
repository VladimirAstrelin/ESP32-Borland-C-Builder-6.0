object Form1: TForm1
  Left = 420
  Top = 249
  Width = 473
  Height = 486
  Caption = 'ESP_EXPERIMENT'
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  OldCreateOrder = False
  OnClose = FormClose
  OnCreate = FormCreate
  PixelsPerInch = 96
  TextHeight = 13
  object GB_ESP_CONNECTION: TGroupBox
    Left = 24
    Top = 16
    Width = 409
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
  object GB_ESP_INVOKE_ANY_PROGRAM: TGroupBox
    Left = 24
    Top = 192
    Width = 409
    Height = 217
    Caption = 'ESP INVOKE ANY WINDOWS PROGRAM'
    TabOrder = 1
    object LBL_D4_PINNED_TO: TLabel
      Left = 104
      Top = 24
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
      Left = 104
      Top = 87
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
    object LBL_D17_PINNED_TO: TLabel
      Left = 104
      Top = 152
      Width = 104
      Height = 13
      Caption = 'D17 PINNED TO :'
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
      Height = 41
      Caption = 'PIN TO D4'
      TabOrder = 0
      OnClick = BTN_PIN_TO_D4Click
    end
    object BTN_PIN_TO_D16: TButton
      Left = 8
      Top = 87
      Width = 75
      Height = 42
      Caption = 'PIN TO D16'
      TabOrder = 1
      OnClick = BTN_PIN_TO_D16Click
    end
    object EDIT_D4: TEdit
      Left = 104
      Top = 40
      Width = 289
      Height = 21
      ReadOnly = True
      TabOrder = 2
    end
    object EDIT_D16: TEdit
      Left = 104
      Top = 104
      Width = 289
      Height = 21
      ReadOnly = True
      TabOrder = 3
    end
    object BTN_PIN_TO_D17: TButton
      Left = 8
      Top = 152
      Width = 75
      Height = 41
      Caption = 'PIN TO D17'
      TabOrder = 4
      OnClick = BTN_PIN_TO_D17Click
    end
    object EDIT_D17: TEdit
      Left = 104
      Top = 168
      Width = 289
      Height = 21
      TabOrder = 5
    end
  end
  object SB_MAIN_STATUS_BAR: TStatusBar
    Left = 0
    Top = 428
    Width = 457
    Height = 19
    Panels = <>
    SimplePanel = False
  end
  object TimerReadCom: TTimer
    Enabled = False
    Interval = 50
    OnTimer = TimerReadComTimer
    Left = 440
    Top = 24
  end
  object OpenDialog1: TOpenDialog
    Left = 440
    Top = 56
  end
end
