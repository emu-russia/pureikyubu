
    Dolwin DVD Plugin Readme.

    This is open source plugin for plugin authors. I moved whole DVD code
    in stand-alone plugin, because it is platform specific and may contain 
    many features, like support for various compressing formats.

    Currently, Dolwin DVD is supporting only my own "GMP" format, which is
    using LZW encoding with caching of decoded blocks. It is not good
    idea to add new formats, because user will get more compressed files 
    for one GCM. Although, plugin may contain support for various format, to
    let user decide between them.

    GMP format file can be detected by "GCM_CMPR" string at the beginning
    of it. Do not use file extension to detect format !!

    You are free to experiment with it. See details in DolwinPluginSpecs.h.
    Please, write about all modifications, bugs or new ideas to me.

    Last edited 18 Aug 2004
    org <kvzorganic@mail.ru>
