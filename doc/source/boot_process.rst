Boot Process
============

This document describes the main boot flow. The boot process is modular
and can support many different use cases.

.. uml::

    start
    :Power on reset;
    :Arch init;
    :Plat init;
    :Board init;
    if (Board init succesful) then (yes)
        :Boot init;
        :Board early boot init;
        if (Board early succesful) then (yes)
            :asdf;
        else (no)
            :Initialize command mode;
        endif
    else (no)
        :Initialize command mode;
    endif
    stop
