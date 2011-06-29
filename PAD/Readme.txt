    
    Dolwin PAD Plugin Readme.

    This is simple PAD plugin for GC controller. It is using GetAsyncKeyState
    to retrieve keyboard buttons status. I just integrated old PAD plugin code
    with Dolwin project, for easy compilation.

    Configuration code is looking messy, but there is no better way to handle
    GC controller.

    Known bugs : '?' on key config sometimes. Dont know all details about
    clamping/real stick values. Should we use PADSpec ? Should we use 'origins'
    in some way ? How about other GC serial devices ?

    This plugin can be used as example, for advanced PAD plugins.

    Last edited 14 Nov 2004
    org <kvzorganic@mail.ru>
