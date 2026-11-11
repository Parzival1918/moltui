Set Shell "bash"
Set FontSize 14
Set Width 1150
Set Height 700
Set Padding 6
Set CursorBlink false
Set TypingSpeed 10ms

Type "clear && ./build/moltui -p -m half-blocks --rotate 4 3 -W 118 -H 34 tests/data/benzene.xyz"
Enter
Sleep 1s
Screenshot docs/assets/example_benzene.png
Sleep 500ms

Type "clear && ./build/moltui -p -m half-blocks -u --hkl 1 0 1 --rotate 3 2 -W 118 -H 34 tests/data/structures.cif"
Enter
Sleep 1s
Screenshot docs/assets/example_crystal_unwrapped.png
Sleep 500ms

Type "clear && ./build/moltui -p -m half-blocks -u --cell 2 2 2 -v b -W 118 -H 34 tests/data/structures.cif"
Enter
Sleep 1s
Screenshot docs/assets/example_supercell_b.png
Sleep 500ms

Type "clear && ./build/moltui -p -m ascii --rotate 4 3 -W 110 -H 32 tests/data/benzene.xyz"
Enter
Sleep 1s
Screenshot docs/assets/example_ascii.png
Sleep 500ms
