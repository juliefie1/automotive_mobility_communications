# Automotive mobility communications

The intelligent mobility chair project, launched by the INSA Toulouse fondation with Actia, aims to associate variate industrialists to experiment innovative technologies in a constant evolution of technological and societal challenges.

As part of this project, INSA and Actia came together with Sierra Wireless and Kinéis to implement cellular and satellite communications on board the chair. Kineis' KIM1 expansion module for Sierra Wireless' mangOH open source hardware platform for IoT is able to include satellite communications. 
One of the objectives of this project is to enable the system to switch to satellite communication when the cellular network is no longer available.

## Documentations 
The User Guide is available for mangOH [Yellow](https://mangoh.io/uploaded-documents/Reference/mangOH%20Yellow/Discover/Hardware%20User%20Guide/mangOH_Yellow_User_Guide.pdf) as well as mangOH [Red](https://mangoh.io/uploaded-documents/Reference/mangOH%20Red/Discover/Hardware%20User%20Guide/mangOH_Red_User_Guide.pdf).
A Getting Started Guide is also available for mangOH [Yellow](https://mangoh.io/uploaded-documents/Reference/mangOH%20Yellow/Discover/Getting%20Started/mangOH_Yellow_GSG.pdf) and [Red](https://mangoh.io/uploaded-documents/Reference/mangOH%20Red/Discover/Getting%20Started/mangOH_Red_GSG_nonOctave.pdf).

All the information concerning Legato, the application framework of mangOH boards, is available [here](https://docs.legato.io/latest/getStarted.html).

Legato offers a workspace manager, [Leaf](https://docs.legato.io/latest/confLeaf.html), making it easier to install the toolchain and set the environement.

To know more about Kinéis' system, please ask them for more information.

## YellowSensorToCloud
Components and applications interacting with sensors on the mangOH Red, WP77XX, and pushing data to Sierra Wireless's cloud service, Airvantage. 
Same mechanism as the RedSensorToCloud package, but adapted to the mangOH Yellow board. More sensors can be added in the components/sensors file to be sent to the Data Hub. 
The two applications (.adef files) are : 
- redSensor: Collects data from the mangOH Yellow's imu and position sensors and sends it to the Legato Data Hub. Also sends the cellular signal quality to the Data Hub.
- redCloud: Takes data from the Data Hub and pushes it to AirVantage.

## RedSensorToCloud
Based on the [classic package](https://github.com/mangOH/RedSensorToCloud), to which is added the system for sending data to ArgosWeb (Kinéis’ cloud).
The argosPublisher file contains all the librairies and file necessary to send data through the KIM1 IoT Card, as well as the argosPublisher component. Data is being sent in the argos_publisher function in argosPublisher.c.
The dataPublisher file contains the file where data is being taken from to Data Hub to be pushed to the cloud.

## Automatic Switch
Unfortunately, the automatic switch mechanism is not ready yet. 
When the cellular communication is not available anymore, the AirVantage session is stopped and AvSessionStateHandler is called. For now, the argos_publisher function is called in that handler when the AirVantage session is stopped. It is not possible to do it that way : such a function can’t be called in a handler. 

A possible way to implement the automatic switch would be to just call the argosPublisher component in the AvSessionStateHandler, so that sending data through the KIM1 would be done in another process, in parallel. This would work as a switch, since the argos_publisher only tries to send data 5 times then closes the KIM1, and that the handler manages whenever the AirVantage session is starting again.

In order to observe all data in the same place, coming from the cellular network or from the satellite network (ArgosWeb), an option would be to use Octave, by sending Kinéis’ raw data to Octave in a stream thanks to a component calling Octave’s API, and then decode it thanks to an edge action that would send the decoded data to a new stream. 
