# ninebot-legaliseren
Legaliseer Ninebot by Segway ES1LD voor 't gebruik als e-bike op de openbare weg.

# Hoe sluit ik de Arduino Nano aan?
![Wiring Scheme](schema.png?raw=true "Wiring Scheme")

# Hoe zet ik de firmware in de Arduino?
Installer de [Arduino IDE](https://www.arduino.cc) en selecteer oa. de Nano onder 'Tools' als 'Board' en zet de juiste poort.
Open de .ino file uit deze repository en selecteer vervolgens 'Upload' onder 'Sketch' menu.

# Uitleg
Onder de 'SPEED_MIN' km/h doet de ondersteuning niks.

Bij een 'kick' (= afzetje) en het verschil tussen stilstand en rijden is >= 3km/h dan start de ondersteuning.

Na 'DELAY_KEEP_THROTTLE' ms, indien er geen enkele kick is gegeven bij welke ondersteuning ook, zal de snelheid weer worden afgebouwd.

Of als de snelheid onder de 'SPEED_MIN' km/h komt, stop de ondersteuning; bijv bij remmen, maar dit kan ook bij stijle heuvels.

### Stap 1
De (eerste) 'kick' snelheid zal vast worden gezet (waarschijnlijk ±7km/h) en voor 'DELAY_SPEED_DETECTION' ms worden opgebouwd.
In deze tijd kan een 2e kick worden gegeven om de snelheid te verlengen.

### Stap 2

Wacht je na Stap 1 > 'DELAY_SPEED_DETECTION' ms of geef je in de tussentijd 'KICKS_RESET_TRESHOLD' extra kick's dan zal zal de Stap 1 cyclus worden herhaalt met de hogere snelheid van dat moment.

Simpel. 'trage' hardere kicks versnelt & veel korte kicks achter elkaar.

Remmen? gebruik gewoon de aanwezige remmen.


# Ministerie van Infrastructuur en Waterstaat
> Voor e-steps met trapondersteuning gelden dezelfde regels als voor een e-bike: u moet zelf meetrappen om vooruit te komen, de trapondersteuning stopt bij 25 kilometer per uur en het maximale motorvermogen is 250 watt. De step mag dus niet zelfrijdend zijn. Zie ook https://www.rijksoverheid.nl/onderwerpen/fiets/vraag-en-antwoord/welke-regels-gelden-voor-mijn-elektrische-fiets-e-bike.  
> 
> E-steps met trapondersteuning moeten ook voldoen aan de 'permanente eisen' voor fietsen zoals die staan in de Regeling voertuigen (hoofdstuk 5, afdeling 9): https://wetten.overheid.nl/jci1.3:c:BWBR0025798&hoofdstuk=5&afdeling=9&z=2021-01-21&g=2021-01-21.  
> 
> We hopen u hiermee voldoende te hebben geïnformeerd.
> 
> Met vriendelijke groet,
> 
> Ministerie van Infrastructuur en Waterstaat
> DGMo | Burgerbrieven 
