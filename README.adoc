`LADI jackdbus <https://github.com/LADI/jackdbus>`_
################################

.. image:: https://github.com/LADI/jackdbus/actions/workflows/build.yml/badge.svg
   :target: https://github.com/LADI/jackdbus/actions
.. image:: https://repology.org/badge/tiny-repos/jack-audio-connection-kit.svg
   :target: https://repology.org/metapackage/jack-audio-connection-kit/versions

jackdbus was created initially for JACK1.
In 2009, jackdbus was ported to
`JACK2 <http://lac.linuxaudio.org/2009/cdm/Thursday/01_Letz/01.pdf>`_
along with implementation of the new control API for JACK2.
As of 2022, jackdbus is maintained in LADI project.
The jackaudio.org version of jackdbus is considered older/alternative
implementation.

For LADI specific issues, submit issues or pull request to LADI project.
For related discussions, you are invited to join
`Libera.Chat <https://libera.chat/>`_ channel #ladi

Do not submit LADI specific issues to jackaudio project.


D-Bus access
############

D-Bus is an object model that provides IPC
mechanism. D-Bus supports autoactivation of
objects, thus making it simple and reliable to
code a ”single instance” application or daemon,
and to launch applications and daemons on de-
mand when their services are needed.

Improvements over classical “jackd” approach:

 • Simplified single thread model for con trol and monitor
   applications. Various D-Bus language bindings make it trivial to
   write control and monitor applications using scripting languages
   like Python, Ruby, Perl, etc..
 • A log file is available for inspection even when autoactivation
   happens by the first launched JACK application.
 • A real configuration file is used to persist settings to be
   manipulated through configuration interface of JACK D-Bus object.
 • Improved graph inspection and control mechanism. JACK graph is
   versioned. Connections, ports and clients have unique
   (monotonically increasing) numeric IDs.
 • High level abstraction of JACK settings. Allows applications that
   can configure JACK to expose parameters that were not known at
   compile (or tarball release) time. Recent real world examples are
   the JACK MIDI driver parameters and support for FFADO driver in
   QJackCtl. Upgrading of JACK requires upgrade of QJackCtl in order
   to make new settings available in the GUI.

3.2 How it works
3.2.1 Autoactivation and starting/stopping JACK server

First, application that issues D-Bus method
call to JACK controller object, causes
D-Bus session daemon to activate the ob-
ject by starting the jackdbus executable.
Activating controller object does not start
the server. Instead controller object has
several interfaces. The most important of
them is the control interface. Control interface
contains methods for starting and stopping
JACK server, loading and unloading of internal
clients (netjack), setting buffer size and reset-
ting xrun counter. It also contains methods for
querying information required by monitoring
applications: whether JACK server is started,
whether JACK server is running in realtime
mode, sample rate, DSP load, current buffer
size, latency, xrun counter.
JACK server autostart is achieved by lib-
jack calling “jack server start” method of
JACK control D-Bus interface.

3.2.2 JACK settings Applications that want to manage

JACK settings can query and set all set-
tings that were traditionally specified as
jackd command-line parameters. Interface
abstraction provides virtual tree of parameter
containers with container leaves that contain
parameters. Parameters are accessed using
simple addressing scheme (array of strings)
where address defines path to parameter, like
“drivers”, “alsa”, “dither”.

Overview of the tree of standard settings’ ad-
dresses:
• ”engine”
• ”engine”, ”driver”
• ”engine”, ”realtime”
• ”engine”, ...more engine parameters
• ”driver”, ”device”
• ”driver”, ...more driver parameters
• ”drivers”, ”alsa”, ”device”
• ”drivers”, ”alsa”, ...more alsa driver pa-
rameters
• ”drivers”, ...more drivers
• ”internals”, ”netmanager”, ”multicast ip”
• ”internals”, ”netmanager”, ...more net-
manager parameters
• ”internals”, ...more internals
JACK settings are persisted. I.e. they are au-
tomatically saved by jackdbus when they are
set. Next time user starts JACK server, last
saved settings will be automatically used.
Changing JACK settings through the con-
figure D-Bus interface takes effect on next
JACK server start. On the fly change of
the buffer size, as available in the libjack
(jack.h) API, is also possible through the con-
trol D-Bus interface.

3.2.3 JACK parameter constraints

JACK internal modules that provide parame-
ters visible through control API can provide
information about parameter valid range (like
realtime priority) or whether parameter should
be presented as enumeration. Enumeration pa-
rameters can be strict and non-strict. Exam-
ple of strict enum parameter is dither parame-
ter of ALSA driver, it has only predefined valid
values - “shaped noise”, “rectangular”, “trian-
gualr” and “none”. Example of non-strict pa-
rameter is device parameter of ALSA driver. It
is useful to provide some detected device strings
as choices to user, but still allow user to specify
custom string that ALSA layer is supposed to
understand.

3.2.4 JACK patchbay

In order to simplify patchbay applications, ex-
tended functionality is provided. There is a
method that returns the current graph state.
Graph state has unique monotonically increas-
ing version number and contains info about all
clients, their ports and connections. Connec-
tions, ports and clients have unique numeric IDs
that are guaranteed not to be reused. Notifica-
tions about graph changes are provided using
D-Bus signals.

3.3 JACK D-Bus enabled applications
• JACK contains “jack control” executable
- a 300 lines of Python exposing
JACK D-Bus functionality. It allows
chained execution of several commands.
For example jack control ds alsa dps
midi-driver raw eps realtime on
eps relatime-priority 70 start se-
lects ALSA driver, enables JACK MIDI
raw backend, enables realtime mode,
sets realtime priority to 70 and starts
JACK server.
• LADI Tools is a set of programs to config-
ure, control and monitor JACK . It pro-
vides tray icon, Window Maker style dock-
app, G15 keyboard LCD display integra-
tion application, configuration utility for
managing JACK settings and log file mon-
itor application. All tools are written in
Python.
• Patchage, the ubiquitous canvas modu-
lar patch bay can be compiled to use
D-Bus instead of libjack to communicate
with JACK . Doing so also enables
JACK server start/stop functionality in
Patchage.
• LASH, recent developments of the audio
session handler by default use D-Bus to
communicate with JACK . Various
JACK related features are planned:
– Saving of JACK settings as part of
“studio” session.
– Handling of “JACK server crash”
scenario: restarting JACK server,
notifying JACK applications that
JACK server reappeared so they
can reconnect to it, and restoring
JACK connections.
