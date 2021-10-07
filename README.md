# SY1000
VST3 Plugin to control the BOSS SY1000 Guitar Synthesizer with Gig Performer
 
I programmed this plugin using the JUCE framework for Gig Performer (https://www.gigperformer.com). 
It allows direct control of almost all parameters of the Boss SY1000 guitar synthesizer via MIDI SysEx messages. 
The various parameters of the SY1000 can be controlled as "virtual" VST plug-in parameters from Gig Performer. 

One or more parameter values of the SY1000 can be easily modified with rackspaces or rackspace variations.
This is done by simply assigning the corresponding virtual plugin parameter to a Gig Performer widget. 

The changes to the plugin parameter values are then transmitted directly to the SY1000 via a MIDI SysEx message. 
This is also possible in the reverse direction. 
Parameter changes directly on the SY1000 device are transmitted to the plugin via SysEx messages and the value of the corresponding "virtual" plugin parameter is updated accordingly. 

Instructions and screenshots can be found in the Gig Performer Community Forum.
