
( system calls )

: emit    0 sys ;
: .       1 sys ;
: tell    2 sys ;
: quit    128 sys ;
: sin     129 sys ;
: include 130 sys ;
: save    131 sys ;


( dictionary access. These are shortcuts through the primitive operations are !!, @@ and ,, )

: !    0 !! ;
: @    0 @@ ;
: ,    0 ,, ;
: #    0 ## ;


( compiler state )

: [ 0 compiling ! ; immediate
: ] 1 compiling ! ;
: postpone 1 _postpone ! ; immediate


( some operators and shortcuts )

: over 1 pick ;
: +!   dup @ rot + swap ! ;
: inc  1 swap +! ;
: dec  -1 swap +! ;
: <    - <0 ;
: >    swap < ;
: <=   over over >r >r < r> r> = + ;
: >=   swap <= ;
: =0   0 = ;
: not  =0 ;
: !=   = not ;
: cr   10 emit ;
: ..   dup . ;
: here h @ ;


( memory management )

: allot  h +!  ;
: var : postpone [ ' lit , here dup 5 + , ' exit , here swap ! 5 allot ;


( 'begin' gets the current address, a jump or conditional jump back is generated
  by 'again', 'until' or 'times' )

: begin   here ; immediate
: again   ' jmp , , ; immediate
: until   ' jmp0 , , ; immediate
: times ' 1 - , ' dup , ' =0 , postpone until ; immediate


( 'if' prepares conditional jump, address will be filled in by 'else' or 'fi' )

: if      ' jmp0 , here 999 , ; immediate
: unless  ' not , postpone if ; immediate
: else    ' jmp , here 999 , swap here swap ! ; immediate
: fi      here swap ! ; immediate


( forth style 'do' and 'loop', including loop iterators 'i' and 'j' )

: i ' lit , 0 , ' pickr , ; immediate
: j ' lit , 2 , ' pickr , ; immediate
: do ' swap , ' >r , ' >r , here ; immediate
: loop+ ' r> , ' + , ' dup , ' >r , ' lit , 1 , ' pickr , ' > , ' jmp0 , , ' r> , ' drop , ' r> , ' drop , ; immediate
: loop ' lit , 1 , postpone loop+ ;  immediate


( Create string literal, puts length and address on the stack )

: s" compiling @ if ' lits , here 0 , fi here begin key dup 34 = if drop
     compiling @ if here swap - swap ! else dup here swap - fi exit else , fi
     again ; immediate

( Print string literal )

: ." compiling @ if postpone s" ' tell , else begin key dup 34 = if drop exit else emit fi again
     fi ; immediate


." Welcome to zForth, " here . ." bytes used" cr ;


(
vi: ts=3 sw=3 ft=forth
)


