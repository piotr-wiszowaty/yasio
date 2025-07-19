[text-section] init

[code]
start equ $2000
 opt h+f-
 org start
 cli
[end-code]

[text-section] text

$022F constant sdmctl
$0230 constant dladr
$0300 constant ddevic
$0301 constant dunit
$0302 constant dcmnd
$0303 constant dstats
$0304 constant dbufa
$0306 constant dtimlo
$0307 constant dunuse
$0308 constant dbyt
$030A constant daux1
$030B constant daux2
$4000 constant screen
$43C0 constant sio-buffer

$40 constant sio-read
$80 constant sio-write

$F0 constant cmd-get-ip-address
$F1 constant cmd-get-network
$F2 constant cmd-select-network
$F3 constant cmd-get-wifi-mode
$F4 constant cmd-scan-networks
$F5 constant cmd-get-ap-parameters
$F6 constant cmd-set-ap

80 constant wifi-mode-origin
160 constant sta-ip-address-origin
240 constant networks-msg-origin
280 constant networks-origin
760 constant sta-password-msg-origin
880 constant legend0-origin
920 constant legend-origin

160 constant ap-ssid-origin
200 constant ap-password-msg-origin
240 constant ap-ip-address-origin
800 constant error-msg-origin

$00 constant wifi-mode-sta
$FF constant wifi-mode-ap

10 constant max-networks

1 constant ap-field-ssid
2 constant ap-field-password
3 constant ap-field-ip

32 constant ap-ssid-size
32 constant ap-password-size
20 constant ap-ip-address-size

variable cursor
create blank-line ,'                                         '
create title  ,' - YASIO setup --'
create wifi-mode-msg ,' WiFi mode: [ ] Station  [ ] Access Point'
create ssid-msg ,' SSID:                                   '
create ip-msg ,' IP address:                             '
create networks-msg ,' Networks:                               '
create networks-pending-msg ,' Networks: (scanning)                    '
create password-msg ,' Password:'
create password-entry-msg 40 c, $5F c, '                                       ' $5E c,
create invalid-ip-address-msg ,' Invalid IP address. Should be A.B.C.D/M'
create legend0 40 c, ' Tab'* ' :change WiFi mode                    '
create legend1 40 c, $DC c, $DD c, ' :move ' ' Space'* ' :refresh IP ' ' Return'* ' :select    '
create legend2 40 c, ' Return'* ' :connect ' ' Esc'* ' :cancel               '
create legend3 40 c, $DC c, $DD c, ' :move ' ' Return'* ' :confirm                  '
create password 40 allot
create wifi-mode wifi-mode-sta ,
create network-index 0 ,
create n-networks 0 ,
create ap-field 0 ,
create ap-ssid 0 c, ap-ssid-size allot
create ap-password 0 c, ap-password-size allot
create ap-ip-address 0 c, ap-ip-address-size allot
create dlist
 $70 c, $70 c, $70 c,
 $42 c, screen ,
 $02 c, $02 c, $02 c, $02 c, $02 c, $02 c,
 $02 c, $02 c, $02 c, $02 c, $02 c, $02 c,
 $02 c, $02 c, $02 c, $02 c, $02 c, $02 c,
 $02 c, $02 c, $02 c, $02 c, $02 c, $41 c, dlist ,
create passwords 64 max-networks * allot

: get-char    ( -- c )
[code]
 lda #0
 dex
 sta pstack,x
 stx w
 jsr do_gc
 ldx w
 dex
 sta pstack,x
 jmp next
do_gc
 lda $E425
 pha
 lda $E424
 pha
 rts
[end-code] ;

: ascii-to-internal ( c -- c )
[code]
 lda pstack,x
 tay
 lda a2i_lut,y
 sta pstack,x
 jmp next

a2i_lut
;     00  01  02  03  04  05  06  07  08  09  0A  0B  0C  0D  0E  0F
 dta $40,$41,$42,$43,$44,$45,$46,$47,$48,$49,$4A,$4B,$4C,$4D,$4E,$4F ; 00..0F
 dta $50,$51,$52,$53,$54,$55,$56,$57,$58,$59,$5A,$5B,$5C,$5D,$5E,$5F ; 10..1F
 dta $00,$01,$02,$03,$04,$05,$06,$07,$08,$09,$0A,$0B,$0C,$0D,$0E,$0F ; 20..2F
 dta $10,$11,$12,$13,$14,$15,$16,$17,$18,$19,$1A,$1B,$1C,$1D,$1E,$1F ; 30..3F
 dta $20,$21,$22,$23,$24,$25,$26,$27,$28,$29,$2A,$2B,$2C,$2D,$2E,$2F ; 40..4F
 dta $30,$31,$32,$33,$34,$35,$36,$37,$38,$39,$3A,$3B,$3C,$3D,$3E,$3F ; 50..5F
 dta $60,$61,$62,$63,$64,$65,$66,$67,$68,$69,$6A,$6B,$6C,$6D,$6E,$6F ; 60..6F
 dta $70,$71,$72,$73,$74,$75,$76,$77,$78,$79,$7A,$7B,$7C,$7D,$7E,$7F ; 70..7F
 dta $C0,$C1,$C2,$C3,$C4,$C5,$C6,$C7,$C8,$C9,$CA,$CB,$CC,$CD,$CE,$CF ; 80..8F
 dta $D0,$D1,$D2,$D3,$D4,$D5,$D6,$D7,$D8,$D9,$DA,$DB,$DC,$DD,$DE,$DF ; 90..9F
 dta $80,$81,$82,$83,$84,$85,$86,$87,$88,$89,$8A,$8B,$8C,$8D,$8E,$8F ; A0..AF
 dta $90,$91,$92,$93,$94,$95,$96,$97,$98,$99,$9A,$9B,$9C,$9D,$9E,$9F ; B0..BF
 dta $A0,$A1,$A2,$A3,$A4,$A5,$A6,$A7,$A8,$A9,$AA,$AB,$AC,$AD,$AE,$AF ; C0..CF
 dta $B0,$B1,$B2,$B3,$B4,$B5,$B6,$B7,$B8,$B9,$BA,$BB,$BC,$BD,$BE,$BF ; D0..DF
 dta $E0,$E1,$E2,$E3,$E4,$E5,$E6,$E7,$E8,$E9,$EA,$EB,$EC,$ED,$EE,$EF ; E0..EF
 dta $F0,$F1,$F2,$F3,$F4,$F5,$F6,$F7,$F8,$F9,$FA,$FB,$FC,$FD,$FE,$FF ; F0..FF
[end-code] ;

: dump-stack ( addr -- )
[code]
 lda pstack,x
 inx
 clc
 adc #<screen
 sta w        ; screen addr
 lda pstack,x
 inx
 adc #>screen
 sta w+1
 lda #$F0
 sta z        ; input offset
 lda #0
 sta z+1      ; output offset
 lda #8
 sta tmp      ; word counter
dump_stack_loop
; digit 3
 ldy z
 lda pstack+1,y
 jsr tohex_hi
 ldy z+1
 sta (w),y
 inc z+1
; digit 2
 ldy z
 lda pstack+1,y
 jsr tohex_lo
 ldy z+1
 sta (w),y
 inc z+1
; digit 1
 ldy z
 lda pstack,y
 jsr tohex_hi
 ldy z+1
 sta (w),y
 inc z+1
; digit 0
 ldy z
 lda pstack,y
 jsr tohex_lo
 ldy z+1
 sta (w),y
 inc z+1
; next word
 inc z
 inc z
 inc z+1
 dec tmp
 bne dump_stack_loop
 jmp next
tohex_lo
 and #$0F
 jmp tohex
tohex_hi
 lsr @
 lsr @
 lsr @
 lsr @
tohex
 cmp #$0A
 bcc no_adjust
 adc #$6
no_adjust
 adc #$10
 cpx z
 bne normal
 ora #$80
normal
 rts
[end-code] ;

: put-digit     ( c-addr c -- )
  $0F and
  dup 9 > if 23 else 16 then +
  swap c! ;

: show-byte ( u c -- )
  2dup
  4 rshift swap screen + swap put-digit
  swap screen + 1+ swap put-digit ;

: show-bytes ( u1 addr u2 -- )
  0 do
    2dup c@ show-byte
    swap 3 + swap
    1+
  loop
  2drop ;

\ u1 - offset
\ u2 - word to show
: show-word ( u1 u2 -- )
  2dup
  8 rshift show-byte
  swap 2 + swap show-byte ;

: show-words ( u1 addr u2 -- )
  0 do
    2dup @ show-word
    2 + swap 5 + swap
  loop
  2drop ;

: cursor-next   ( -- u )
  cursor @ dup 1+ cursor ! ;

: set-cursor    ( u -- )
  screen + cursor ! ;

: put-char      ( c -- )
  cursor-next c! ;

: set-char      ( c -- )
  cursor @ c! ;

: print-str     ( c-addr -- )
  dup c@ if
    dup count cursor @ swap cmove
    c@ cursor @ + cursor !
  else
    drop
  then ;

: @print-str    ( c-addr u -- )
  [label] at_print_str
  set-cursor
  print-str ;

: print-ascii-str ( c-addr -- )
  dup c@ if
    count 0 do dup i + c@ ascii-to-internal put-char loop
    drop
  else
    drop
  then ;

: @print-ascii-str ( c-addr u -- )
  [label] at_print_ascii_str
  set-cursor
  print-ascii-str ;

: highlight     ( u -- )
  dup c@ $80 or swap c! ;

: unhighlight   ( u -- )
  dup c@ $7F and swap c! ;

: jsiov
[code]
 txa
 pha
 jsr $E459
 pla
 tax
 lda #0
 dex
 sta pstack,x
 tya
 dex
 sta pstack,x
 jmp next
[end-code] ;

\ send/receive data via SIO interface
\ u1 - aux
\ u2 - $40:read, $80:write
\ u3 - data address
\ u4 - data length
\ u5 - command
: sio-command   ( u1 u2 u3 u4 u5 -- n )
  dcmnd c!
  dbyt !
  dbufa !
  dstats c!
  daux1 !
  $31 ddevic c!
  $01 dunit c!
  $08 dtimlo c!
  jsiov ;

: get-ip-address
  0 sio-read sio-buffer 128 cmd-get-ip-address sio-command drop
  sio-buffer
  sta-ip-address-origin 12 +
  @print-ascii-str ;

: get-wifi-mode
  0 sio-read sio-buffer 128 cmd-get-wifi-mode sio-command drop
  sio-buffer c@ wifi-mode ! ;

: get-networks
  0 n-networks !
  networks-origin 2 +
  max-networks 0 do
    i sio-read sio-buffer 128 cmd-get-network sio-command drop
    sio-buffer c@ 0= if leave then
    sio-buffer over @print-ascii-str
    40 +
    sio-buffer 64 + passwords i 6 lshift + 64 cmove
    n-networks @ 1+ n-networks !
  loop
  drop
  0 network-index ! ;

: show-network-selector
  n-networks @ 0 > if
    networks-origin
    n-networks @ 0 do
      dup set-cursor
      40 +
      network-index @ i = if $5F else $00 then put-char
    loop
    drop
  then ;

: next-network
  network-index @ 1+ n-networks @ = not if
    network-index @ 1+ network-index !
    show-network-selector
  then ;

: prev-network
  network-index @ if
    network-index @ 1- network-index !
    show-network-selector
  then ;

: esc-or-return ( c -- flag )
  dup $1B = swap $9B = or ;

: highlight-cursor
  cursor @ highlight ;

: unhighlight-cursor
  cursor @ unhighlight ;

: connect
  sio-buffer 128 erase
  password sio-buffer 64 cmove
  network-index @ sio-write sio-buffer 128 cmd-select-network sio-command drop ;

: select-network
  ip-msg sta-ip-address-origin @print-str
  password-msg sta-password-msg-origin @print-str
  password-entry-msg sta-password-msg-origin 40 + @print-str
  legend2 legend-origin @print-str
  network-index @ 6 lshift passwords + password 40 cmove
  password c@ if
    \ password known
    password sta-password-msg-origin 41 + @print-ascii-str
  else
    \ password unknown
    sta-password-msg-origin 41 + set-cursor
  then
  highlight-cursor
  begin
    get-char
    dup $9B = if                    \ [Return]
      ip-msg sta-ip-address-origin @print-str
      connect
    then
    dup $7E = if                    \ [Delete]
      password c@ if
        unhighlight-cursor
        password c@ 1- password c!
        0 password c@ 1+ password + c!
        cursor @ 1- cursor !
        0 set-char
        highlight-cursor
      then
    then
    dup $20 >= over $7E < and if
      password c@ 38 < if
        dup password c@ password 1+ + c!
        dup ascii-to-internal put-char
        highlight-cursor
        password c@ 1+ password c!
      then
    then
    esc-or-return
  until
  sta-password-msg-origin screen + 80 erase
  legend1 legend-origin @print-str ;

: show-wifi-mode-selector
  wifi-mode c@ wifi-mode-sta = if
    10 wifi-mode-origin screen + 12 + c!
    0  wifi-mode-origin screen + 25 + c!
  else
    0  wifi-mode-origin screen + 12 + c!
    10 wifi-mode-origin screen + 25 + c!
  then ;

: clear-networks
  networks-origin max-networks 0 do blank-line over @print-str 40 + loop drop ;

: show-sta-view ( f -- )
  passwords [ 64 max-networks * ] literal erase
  ip-msg sta-ip-address-origin @print-str
  legend1 legend-origin @print-str
  if
    networks-pending-msg networks-msg-origin @print-str
    clear-networks
    0 0 0 0 cmd-scan-networks sio-command drop
  then
  networks-msg networks-msg-origin @print-str
  get-ip-address
  get-networks
  show-network-selector ;

: update-ap-cursor
  unhighlight-cursor
  ap-field @ ap-field-ssid = if
    ap-ssid-origin 6 + ap-ssid c@ + set-cursor
  then
  ap-field @ ap-field-password = if
    ap-password-msg-origin 10 + ap-password c@ + set-cursor
  then
  ap-field @ ap-field-ip = if
    ap-ip-address-origin 12 + ap-ip-address c@ + set-cursor
  then
  highlight-cursor ;

: show-ap-view
  ssid-msg ap-ssid-origin @print-str
  ap-ssid ap-ssid-origin 6 + @print-ascii-str
  password-msg ap-password-msg-origin @print-str
  ap-password ap-password-msg-origin 10 + @print-ascii-str
  ip-msg ap-ip-address-origin @print-str
  ap-ip-address ap-ip-address-origin 12 + @print-ascii-str
  clear-networks
  legend3 legend-origin @print-str
  update-ap-cursor ;

: prev-ap-field
  ap-field @
  dup ap-field-ssid = if
    \ do nothing
  then
  dup ap-field-password = if
    ap-field-ssid ap-field !
    update-ap-cursor
  then
  ap-field-ip = if
    ap-field-password ap-field !
    update-ap-cursor
  then ;

: next-ap-field
  ap-field @
  dup ap-field-ssid = if
    ap-field-password ap-field !
    update-ap-cursor
  then
  dup ap-field-password = if
    ap-field-ip ap-field !
    update-ap-cursor
  then
  ap-field-ip = if
    \ do nothing
  then ;

: switch-to-sta-view
  wifi-mode-sta wifi-mode !
  blank-line ap-password-msg-origin @print-str
  show-wifi-mode-selector
  -1 show-sta-view ;

: switch-to-ap-view
  wifi-mode-ap wifi-mode !
  show-ap-view
  show-wifi-mode-selector ;

: get-ap-parameters
  0 sio-read sio-buffer 128 cmd-get-ap-parameters sio-command drop
  sio-buffer ap-ssid 33 cmove
  sio-buffer 33 + ap-password 33 cmove
  sio-buffer 66 + ap-ip-address 19 cmove
  ap-field-ssid ap-field ! ;

: digit? ( c -- f )
  dup $30 >= swap $39 <= and if -1 else 0 then ;

: octet-valid?         ( addr-1 addr-2 -- addr-1 addr-3 f )
  [label] octet_valid_qmark
  2dup u> if
    dup c@ digit? if
      0
      begin
        over c@ digit? 2over u> and while
        10 m* drop over c@ $30 - +
        swap 1+ swap
      repeat
      256 u<
    else
      0
    then
  else
    -1
  then ;

: addrmask-valid?      ( addr-1 addr-2 -- addr-1 addr-3 f ) 
  [label] addrmask_valid_qmark
  2dup u> if
    dup c@ digit? if
      0
      begin
        over c@ digit? 2over u> and while
        10 m* drop over c@ $30 - +
        swap 1+ swap
      repeat
      32 u<
    else
      0
    then
  else
    -1
  then ;

: dot?                 ( addr-1 addr-2 -- addr-1 addr-3 f )
  [label] is_dot_qmark
  2dup u> if
    dup c@ $2E = if
      1+ -1
    else
      0
    then
  else
    -1
  then ;

: slash?               ( addr-1 addr-2 -- addr-1 addr-3 f )
  [label] is_slash_qmark
  2dup u> if
    dup c@ $2F = if
      1+ -1
    else
      0
    then
  else
    -1
  then ;

: >>=                  ( addr-1 addr-2 f1 xt -- addr-1 addr-3 f2 )
  [label] bind
  swap if
    execute
  else
    drop 0
  then ;

: valid-ap-ip-address? ( -- f )
  [label] valid_ap_ip_address_qmark
  ap-ip-address count over + swap -1  \ end+1 start flag
  ['] octet-valid? >>=
  ['] dot? >>=
  ['] octet-valid? >>=
  ['] dot? >>=
  ['] octet-valid? >>=
  ['] dot? >>=
  ['] octet-valid? >>=
  ['] slash? >>=
  ['] addrmask-valid? >>=
  -rot 2drop ;

: main
  $00 sdmctl c!
  dlist dladr !
  $22 sdmctl c!
  $02 $02C6 c!

  title 0 @print-str
  wifi-mode-msg wifi-mode-origin @print-str
  legend0 legend0-origin @print-str

  get-wifi-mode
  show-wifi-mode-selector
  get-ap-parameters
  wifi-mode c@ wifi-mode-sta = if 0 show-sta-view else show-ap-view then

  begin
    get-char
    \ 38 over show-byte
    wifi-mode c@ wifi-mode-sta = if
      dup $1C = if prev-network then       \ [Control] + [Up]
      dup $1D = if next-network then       \ [Control] + [Down]
      dup $9B = if select-network then     \ [Return]
      dup $20 = if get-ip-address then     \ [Space]
      dup $7F = if switch-to-ap-view then  \ [Tab]
    else
      blank-line error-msg-origin @print-str
      update-ap-cursor
      dup $9B = if                         \ [Return]
        valid-ap-ip-address? if
          ap-ssid sio-buffer ap-ssid-size cmove
          ap-password sio-buffer ap-ssid-size + 1+ ap-password-size cmove
          ap-ip-address sio-buffer ap-ssid-size + ap-password-size + 2 + ap-ip-address-size cmove
          0 sio-write sio-buffer 128 cmd-set-ap sio-command drop
        else
          invalid-ip-address-msg error-msg-origin @print-str
        then
        update-ap-cursor
      then
      dup $1C = if prev-ap-field then      \ [Control] + [Up]
      dup $1D = if next-ap-field then      \ [Control] + [Down]
      dup $7F = if switch-to-sta-view then \ [Tab]
      dup $7E = if                         \ [Delete]
        ap-field @ ap-field-ssid = if
          ap-ssid c@ if
            unhighlight-cursor
            ap-ssid c@ 1- ap-ssid c!
            0 ap-ssid c@ 1+ ap-ssid + c!
            cursor @ 1- cursor !
            0 set-char
            highlight-cursor
          then
        then
        ap-field @ ap-field-password = if
          ap-password c@ if
            unhighlight-cursor
            ap-password c@ 1- ap-password c!
            0 ap-password c@ 1+ ap-password + c!
            cursor @ 1- cursor !
            0 set-char
            highlight-cursor
          then
        then
        ap-field @ ap-field-ip = if
          ap-ip-address c@ if
            unhighlight-cursor
            ap-ip-address c@ 1- ap-ip-address c!
            0 ap-ip-address c@ 1+ ap-ip-address + c!
            cursor @ 1- cursor !
            0 set-char
            highlight-cursor
          then
        then
      then
      dup $20 >= over $7E < and if         \ printable character
        ap-field @ ap-field-ssid = if
          ap-ssid c@ 32 < if
            dup ap-ssid c@ ap-ssid 1+ + c!
            dup ascii-to-internal put-char
            highlight-cursor
            ap-ssid c@ 1+ ap-ssid c!
          then
        then
        ap-field @ ap-field-password = if
          ap-password c@ 29 < if
            dup ap-password c@ ap-password 1+ + c!
            dup ascii-to-internal put-char
            highlight-cursor
            ap-password c@ 1+ ap-password c!
          then
        then
        ap-field @ ap-field-ip = if
          ap-ip-address c@ 18 < if
            dup ap-ip-address c@ ap-ip-address 1+ + c!
            dup ascii-to-internal put-char
            highlight-cursor
            ap-ip-address c@ 1+ ap-ip-address c!
          then
        then
      then
    then
    drop
  again ;

[code]
 run start
[end-code]
