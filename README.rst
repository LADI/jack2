`LADI JACK2 <https://github.com/LADI/jack2>`_
################################

.. image:: https://github.com/LADI/jack2/actions/workflows/build.yml/badge.svg
   :target: https://github.com/LADI/jack2/actions
.. image:: https://repology.org/badge/tiny-repos/jack-audio-connection-kit.svg
   :target: https://repology.org/metapackage/jack-audio-connection-kit/versions

This is a variant of `jackaudio.org JACK2 <https://github.com/jackaudio/jack2/>`_
codebase for use with `LADI/jackdbus <https://github.com/LADI/jackdbus>`_ and
other `LADI <https://github.com/LADI/>`_ related stuff.

For LADI specific issues, submit issues or pull request to LADI project.
For related discussions, you are invited to join
`Libera.Chat <https://libera.chat/>`_ channel #ladi

Do not submit LADI specific issues to jackaudio project.

JACK2 aka jackdmp is a C++ version of the JACK low-latency audio server for
multi-processor machines. It is a new implementation of the JACK server core
features that aims at removing some limitations of the JACK1 design. The
activation system has been changed for a data flow model and lock-free
programming techniques for graph access have been used to have a more dynamic
and robust system.

- uses a new client activation model, that allows simultaneous client
  execution (on a SMP machine) when parallel clients exist in the graph (client
  that have the same inputs). This activation model allows to better use
  available CPU on a smp machine, but also works on mono-processor machine.

- uses a lock-free way to access (read/write) the client graph, thus
  allowing connections/disconnection to be done without interrupting the audio
  stream. The result is that connections/disconnections are glitch-free.

- can work in two different modes at the server level:

  - *synchronous activation*: in a given cycle, the server waits for all
    clients to be finished (similar to normal jackd)

  - *asynchronous activation*: in a given cycle, the server does not wait for
    all clients to be finished and use output buffer computed the previous
    cycle.
    The audible result of this mode is that if a client is not activated
    during one cycle, other clients may still run and the resulting audio
    stream will still be produced (even if its partial in some way). This
    mode usually result in fewer (less audible) audio glitches in a loaded
    system.

For further information, see the JACK `homepage <https://jackaudio.org/>`_ and `wiki <https://github.com/jackaudio/jackaudio.github.com/wiki>`_. There are also the #jack and #lad chat channels on `Libera Chat IRC <https://web.libera.chat/#jack>`_.

