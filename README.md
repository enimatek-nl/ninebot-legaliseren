# ninebot-legaliseren
Legaliseer Ninebot by Segway ES1LD voor 't gebruik als e-bike op de openbare weg.

# Hoe sluit ik de Arduino Nano aan?
![Wiring Scheme](schema.png?raw=true "Wiring Scheme")

# Hoe zet ik de firmware in de Arduino?
Installer de [Arduino IDE](https://www.arduino.cc) en selecteer oa. de Nano onder 'Tools' als 'Board' en zet de juiste poort.
Open de .ino file uit deze repository en selecteer vervolgens 'Upload' onder 'Sketch' menu.

# Uitleg
Zet de step op **'S'**-mode, of Tune zelf de THROTTLE_MAP als een andere snelheid is gewenst.
Onder de **SPEED_MIN** (default: 5km/h) doet de ondersteuning niks, dus aan de hand houden is veilig.

## Starten
Bij een 'kick' (= afzetje) en het verschil tussen stilstand en rijden is >= 3km/h dan start de ondersteuning.

De hoogste 'kick' snelheid zal vast worden gezet (waarschijnlijk ±7km/h bij de eerste) en voor **DELAY_SPEED_DETECTION** (default: 2sec) worden opgebouwd.

Na **DELAY_KEEP_THROTTLE** (default: 8sec), indien er geen enkele kick is gegeven bij welke ondersteuning ook, zal de snelheid weer langzaam worden afgebouwd in stappen van **DELAY_STOPPING_THROTTLE** (default: 1sec).

Als de snelheid onder de **SPEED_MIN** (default: 5km/h) komt, stop de ondersteuning; dit kan ook bij hellingen.

## Versnellen
Wacht je **DELAY_SPEED_DETECTION** (default: 2sec) of geef je in de tussentijd **KICKS_RESET_TRESHOLD** (default: 1) extra kick's dan zal zal de snelheidsopbouw-cyclus worden herhaalt met de hogere snelheid van dat moment.

Dus simpel gezegd: steady kicks of veel korte kicks achter elkaar versnelt.

## Remmen
Gebruik gewoon de aanwezige remmen of wacht tot de **DELAY_KEEP_THROTTLE** (default: 8sec) voorbij is.

## Let op!
 Scherpe bochten, dan versnelt de step meer dan gebruikelijk en kan deze zomaar starten of versnellen.

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
