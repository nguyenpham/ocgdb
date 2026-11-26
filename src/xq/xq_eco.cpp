/*
 This file is part of Banksia, a chess and xiangqi tournament manager, and is
 used by Open Chess Game Database Standard (OCGDB).

 Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)

 In OCGDB this file is distributed under the MIT License (MIT), as the rest of
 OCGDB (see the file LICENSE, or http://opensource.org/licenses/MIT). The
 copyright holder relicensed the copy taken from Banksia (GPL-3) for OCGDB.
 */

#include <random>
#include <mutex>
#include <iomanip> // for setprecision
#include <fstream>
#include <iostream>

#include "xq.h"


using namespace banksia;

/// The two numbers in each record are Zobrist keys, not data: the key of the
/// position the opening line reaches, and the key of its left-right mirror.
/// They are only valid for the table in xq/xq_hash.cpp (ZobristTableXq and
/// ZobristPlayer) -- change that table and every key here must be regenerated,
/// or no xiangqi opening is recognised at all.
extern const std::vector<Eco> ecoXqVec;

const std::vector<Eco> ecoXqVec = {
    Eco("A01", -1, "Advisor opening", "", 14630223807570476701ULL, 3092645457501853638ULL),
    Eco("A02", -1, "Edge Horse Opening", "", 10948231293835912835ULL, 10078021710595223021ULL),
    Eco("A03", -1, "Edge Cannon Opening", "", 15082117988055005955ULL, 6307302210866503601ULL),
    Eco("A04", -1, "Riverbank Cannon Opening", "", 15357733709497234634ULL, 16634743187607697519ULL),
    Eco("A05", -1, "Cross-river Cannon Opening", "", 9768546030514274225ULL, 18185886323221877608ULL),
    Eco("A06", -1, "Thundering Cannon Opening", "", 10355763680543441629ULL, 428510737931263956ULL),
    Eco("A07", -1, "Golden Hooked Cannon Opening", "", 6643437365858553577ULL, 15453903214406276064ULL),
    Eco("A08", -1, "Edge Pawn Opening", "", 11036521341620511716ULL, 11871843657514203658ULL),
    Eco("A10", -1, "Elephant Opening 01", "", 1833778035183615641ULL, 13816256198460283035ULL),
    Eco("A10", -1, "Elephant Opening 01", "", 12618728379113475301ULL, 9933908154077920878ULL),
    Eco("A12", -1, "Opposite Direction Elephant Opening", "", 3392200570845550700ULL,
        707463427927894759ULL),
    Eco("A13", -1, "Elephant Opening vs. Left Proper Horse Opening", "", 181399788137858246ULL,
        2294610177685206631ULL),
    Eco("A14", -1, "Elephant Opening", "", 13337348085096370277ULL, 11838739125924668100ULL),
    Eco("A15", -1, "Elephant Opening with 3rd Pawn Advancement vs. Right Proper Horse Opening", "",
        15705247369168220756ULL, 11339256634449530691ULL),
    Eco("A16", -1, "Elephant Opening with 7th Pawn Advancement vs. Right Proper Horse Opening", "",
        9223724143474572770ULL, 14170680321427439861ULL),
    Eco("A20", -1, "Elephant Opening vs. Left Palcorner Cannon", "", 15177476643026385134ULL,
        8227497131299877301ULL),
    Eco("A21", -1, "Elephant Opening vs. Right Palcorner Cannon", "", 15339860300956915639ULL,
        8389900647808098028ULL),
    Eco("A22", -1, "Elephant Opening with Left Proper Horse vs. Right Palcorner Cannon", "",
        13598477208727703982ULL, 9650763432697421795ULL),
    Eco("A23", -1, "Elephant Opening with Left Side Horse Variation vs. Right Palcorner Cannon", "",
        6832582352046575295ULL, 16315771701945044106ULL),
    Eco("A24", -1, "Elephant Opening with Ranked Chariot variation vs. Right Palcorner Cannon", "",
        18402714108470033829ULL, 3096658898036036204ULL),
    Eco("A25", -1, "Elephant Opening with 3rd Pawn Advancement vs. Right Palcorner Cannon", "",
        12970887569534907782ULL, 5582332025017398123ULL),
    Eco("A26", -1, "Elephant Opening with 7th Pawn Advancement vs. Right Palcorner Cannon", "",
        17146513287177730608ULL, 1480859776700447965ULL),
    Eco("A27", -1, "Elephant Opening", "", 13598408850548866495ULL, 11565836615493725826ULL),
    Eco("A28", -1, "Elephant Opening with Screen Horse Variation vs. Left Central Cannon", "",
        5564141026611281072ULL, 14472111028343089307ULL),
    Eco("A28", -1, "Elephant Opening with Screen Horse Variation vs. Left Central Cannon", "",
        15339858048565689254ULL, 5837394206437158797ULL),
    Eco("A29", -1, "Elephant Opening vs. Right Central Cannon", "", 454243129772361856ULL,
        1907433229354654653ULL),
    Eco("A30", -1, "Elephant", "", 7933278616193278600ULL, 14885629617710541779ULL),
    Eco("A31", -1, "Elephant with Right Proper Horse vs. Left Cross Palace Cannon", "",
        11499984373521752967ULL, 12015953550438378954ULL),
    Eco("A32", -1,
        "Elephant with Right Proper Horse and Filed Chariot vs. Left Cross Palace Cannon with 7th Pawn "
        "Advancement",
        "", 3493981718953749539ULL, 11817178976958202002ULL),
    Eco("A33", -1,
        "Elephant with Right Proper Horse and Filed Chariot and Side Cannon Variation vs. Left Cross Palace "
        "Cannon with 7th Pawn Advancement",
        "", 16203714215204889541ULL, 17697813764486163398ULL),
    Eco("A34", -1,
        "Elephant 7th Pawn Advancement with Right Proper Horse vs. Left Cross Palace Cannon with 7th Pawn "
        "Advancement",
        "", 678806090471391652ULL, 14059382090970030755ULL),
    Eco("A35", -1, "Elephant vs. Right Cross Palace Cannon", "", 7519799590378294737ULL,
        14472170590597596298ULL),
    Eco("A36", -1, "Elephant", "", 10739167738891185567ULL, 11879031286236263014ULL),
    Eco("A37", -1, "Elephant with Left Proper Horse vs. 7th Pawn Advancement", "", 18256589562000386950ULL,
        6150298616716374889ULL),
    Eco("A38", -1, "Elephant with 7th Pawn Advancement vs. 7th Pawn Advancement", "", 12402833653009776664ULL,
        14139389862898438231ULL),
    Eco("A39", -1, "Elephant vs. 3rd Pawn Advancement", "", 150057651287807076ULL, 3730916246661254045ULL),
    Eco("A40", -1, "Horse opening", "", 17324445642309498346ULL, 7618147536012899068ULL),
    Eco("A41", -1, "Horse Opening vs. 7th Pawn Advancement", "", 8941642821359773420ULL,
        8277307262034139137ULL),
    Eco("A42", -1, "Horse Opening with side cannon variation vs. 7th Pawn Advancement", "",
        12444902776563853578ULL, 2646613999062872917ULL),
    Eco("A43", -1, "Horse Opening with Palcorner cannon variation vs. 7th Pawn Advancement", "",
        821413985304214765ULL, 5700056623181936475ULL),
    Eco("A44", -1, "Horse Opening with Left Central Cannon variation vs. 7th Pawn Advancement", "",
        13848853299751826944ULL, 10896642110966159824ULL),
    Eco("A45", -1, "Horse Opening with 7th Pawn Advancement vs. 7th Pawn Advancement", "",
        4972430917697735531ULL, 1314240989346495024ULL),
    Eco("A50", -1, "Palcorner Cannon", "", 4334727654798465983ULL, 8547094249987387108ULL),
    Eco("A51", -1, "Palcorner Cannon vs. Left Proper Horse", "", 2872907991203619296ULL,
        15490206701187465240ULL),
    Eco("A52", -1, "Palcorner Cannon", "", 2528528224097742246ULL, 15229136864951561666ULL),
    Eco("A53", -1, "Palcorner Cannon with Sandwiched Horse Variation vs. Right Central Cannon", "",
        1920737212516893772ULL, 5871433118850011275ULL),
    Eco("A54", -1, "Palcorner Cannon vs. 7th Pawn Advancement", "", 12705737109775501497ULL,
        7924821472631489561ULL),
    Eco("A60", -1, "Cross Palace Cannon vs. Others", "", 3412949807815409625ULL, 7343208248390453890ULL),
    Eco("A60", -1, "Cross Palace Cannon vs. Others", "", 3795319017331393926ULL, 14235162631406593150ULL),
    Eco("A62", -1, "Cross Palace Cannon vs. Ranked Chariot", "", 2957197368040951609ULL,
        8203015815215700141ULL),
    Eco("A63", -1, "Cross Palace Cannon", "", 9986899718563003647ULL, 8851675735656381595ULL),
    Eco("A64", -1, "Cross Palace Cannon with Filed Chariot vs. Left Central Cannon", "",
        6480616199073267918ULL, 987081274456513723ULL),
    Eco("A65", -1,
        "Cross Palace Cannon with Filed Chariot vs. Left Central Cannon with ranked chariot variation", "",
        6832475969859080626ULL, 1793924249054490376ULL),
    Eco("A65", 1,
        "Cross Palace Cannon with Filed Chariot vs. Left Central Cannon with ranked chariot variation", "",
        6832475969859080626ULL, 1793924249054490376ULL),
    Eco("B00", -1, "Central Cannon vs. Rare Counters", "", 16431953739939542324ULL, 13675846533262894089ULL),
    Eco("B01", -1, "Central Cannon vs. Right Proper Horse", "", 13109889296045267655ULL,
        14873586274059901007ULL),
    Eco("B02", -1, "Central Cannon vs. Right Proper Horse with early advisor variation", "",
        5434077813280837029ULL, 10103586071868850433ULL),
    Eco("B03", -1, "Central Cannon vs. Mandarin Duck Horse Counter", "", 3248527838233970658ULL,
        6148856615300431798ULL),
    Eco("B04", -1, "Central Cannon vs. Right 3 Step Tiger Counter", "", 2407349230963526520ULL,
        596210729476198883ULL),
    Eco("B05", -1, "Central Cannon vs. Left Proper Horse", "", 18445780424463404907ULL,
        2139062444688657141ULL),
    Eco("B06", -1, "Central Cannon vs. Turtle Back Cannon", "", 15311430322910225785ULL,
        4843648997398513269ULL),
    Eco("B07", -1, "Central Cannon vs. Left Cannon Blockade", "", 9190356002798222063ULL,
        16780770890787810629ULL),
    Eco("B10", -1, "Central Cannon vs. Single Horse Defense", "", 14413804234350752288ULL,
        2594573604064583199ULL),
    Eco("B11", -1, "Central Cannon vs. Palcorner cannon with Single Horse Defense variation", "",
        7601139823031292312ULL, 2731832443373322311ULL),
    Eco("B12", -1, "Central Cannon", "", 10781660672728569318ULL, 16782050149769406425ULL),
    Eco("B13", -1, "Central Cannon with Riverbank Cannon vs. Single Horse Defense with Ranked Chariot", "",
        8256815594427373932ULL, 4334775400133978102ULL),
    Eco("B14", -1, "Central Cannon with 7th Pawn Advancement vs. Single Horse Defense with Ranked Chariot",
        "", 12445310629836765281ULL, 9801567392479102440ULL),
    Eco("B32", -1, "Central Cannon with Quick Left Proper Horse vs. Sandwiched Horse Defense", "",
        16068600639314408993ULL, 18072802728503694406ULL),
    Eco("B41", -1, "56 Cannons with Left Proper Horse vs. Sandwiched Horse Defense", "",
        6747130439729938450ULL, 2134874849773958852ULL),
    Eco("B42", -1, "56 Cannons Left Proper Horse vs. Sandwiched Horse with Right Filed Chariot", "",
        10567101125999223358ULL, 670782823858198109ULL),
    Eco("B43", -1,
        "56 Cannons with Left Proper Horse vs. Sandwiched Horse Defense with Filed chariot and side cannon "
        "variation",
        "", 7959985532881057176ULL, 4530666301511311102ULL),
    Eco("B44", -1,
        "56 Cannons with Left Proper Horse vs. Sandwiched Horse Defense with filed chariot side cannon and "
        "7th Pawn Advancement",
        "", 8542042538540480289ULL, 16404478889490349487ULL),
    Eco("B45", -1, "56 Cannons with left side h vs. Sandwiched Horse Defense", "", 3086012533864028691ULL,
        2334224739882523038ULL),
    Eco("B45", -1, "56 Cannons with left side h vs. Sandwiched Horse Defense", "", 6747130439729938450ULL,
        2134874849773958852ULL),
    Eco("B45", -1, "56 Cannons with left side h vs. Sandwiched Horse Defense", "", 15533631698681305370ULL,
        10061939346490320034ULL),
    Eco("B54", -1,
        "57 Cannons with 3rd Pawn Advancement vs. Sandwiched Horse Defense with 3rd Pawn Advancement", "",
        13269904462717895673ULL, 4078374511771790019ULL),
    Eco("B55", -1,
        "57 Cannons with 3rd Pawn Advancement vs. Sandwiched Horse Defense 3rd Pawn Advancement and right "
        "Pawn ranked cannon variation",
        "", 14802416770825501401ULL, 2536686434339367416ULL),
    Eco("B56", -1,
        "57 Cannons 3rd Pawn Advancement with double pawn sacrifice variation vs. Sandwiched Horse Defense "
        "3rd Pawn Advancement and right pawn ranked cannon variation",
        "", 2250614264791151591ULL, 14494898699927085684ULL),
    Eco("C01", -1, "Central Cannon with 7th File Horse", "", 4020844485977302926ULL, 17859412163024908709ULL),
    Eco("C01", -1, "Central Cannon with 7th File Horse", "", 4020844485977302926ULL, 17859412163024908709ULL),
    Eco("C02", -1,
        "Central Cannon with 7th File Horse with riverbank horse variation vs. Screen Horse Defense", "",
        2818255365667945708ULL, 11207771259733996717ULL),
    Eco("C03", -1, "Central Cannon 7th File Horse with Central/5th Pawn Advancement vs. Screen Horse Defense",
        "", 3428258894290112865ULL, 3019383724643451391ULL),
    Eco("C04", -1,
        "Central Cannon 7th File Horse with Central/5th Pawn Advancement vs. Screen Horse Defense with "
        "double Pawn ranked cannons",
        "", 3349887376663045576ULL, 9737338550423104005ULL),
    Eco("C05", -1, "Central Cannon Left Side Horse", "", 15396016340772911263ULL, 10401702584794456780ULL),
    Eco("C10", -1, "Central Cannon with Right Ranked Chariot", "", 76235186446478615ULL,
        3303495782258301624ULL),
    Eco("C10", -1, "Central Cannon with Right Ranked Chariot", "", 76235186446478615ULL,
        3303495782258301624ULL),
    Eco("C20", -1, "Central Cannon Pawn Ranked Chariot 7th File Horse", "", 3339143735147111886ULL,
        12610883918826292269ULL),
    Eco("C21", -1, "Central Cannon Pawn Ranked Chariot Left Ranked Chariot 7th File Horse", "",
        420724656981512156ULL, 17408027706930858157ULL),
    Eco("C22", -1,
        "Central Cannon Pawn Ranked Chariot Left Ranked Chariot 7th File Horse vs. Screen Horse Defense with "
        "Double-headed snake Pawn formation with high cannon",
        "", 13453675123871903606ULL, 6003183006058249652ULL),
    Eco("C23", -1,
        "Central Cannon Pawn Ranked Chariot Left Ranked Chariot 7th File Horse with 3rd Pawn Exchange vs. "
        "Screen Horse Defense with Double-headed snake Pawn formation with high cannon",
        "", 15733032898538491207ULL, 7663174336615436339ULL),
    Eco("C24", -1,
        "Central Cannon Pawn Ranked Chariot Left Ranked Chariot 7th File Horse with 7th Pawn Exchange vs. "
        "Screen Horse Defense with Double-headed snake Pawn formation with high cannon",
        "", 9485576323800613617ULL, 3723473096654217093ULL),
    Eco("C30", -1,
        "Central Cannon Pawn Ranked Chariot with 7th Pawn Advancement vs. Screen Horse Defense with 7th Pawn "
        "Advancement",
        "", 7238166714343433389ULL, 12833705810715259925ULL),
    Eco("C31", -1,
        "Central Cannon Pawn Ranked Chariot with 7th Pawn Advancement vs. Screen Horse Defense with 7th Pawn "
        "Advancement and Early Advisor Variation",
        "", 14562017775067416288ULL, 2229320005300990092ULL),
    Eco("C31", -1,
        "Central Cannon Pawn Ranked Chariot with 7th Pawn Advancement vs. Screen Horse Defense with 7th Pawn "
        "Advancement and Early Advisor Variation",
        "", 14454720324950859828ULL, 2053631765618284120ULL),
    Eco("C32", -1,
        "Central Cannon Pawn Ranked Chariot with 7th Pawn Advancement vs. Screen Horse Defense with 7th Pawn "
        "Advancement and Early Elephant Variation",
        "", 15140956485858907857ULL, 9546255078070194912ULL),
    Eco("C32", -1,
        "Central Cannon Pawn Ranked Chariot with 7th Pawn Advancement vs. Screen Horse Defense with 7th Pawn "
        "Advancement and Early Elephant Variation",
        "", 5913865526044983896ULL, 321482367388075625ULL),
    Eco("C33", -1,
        "Central Cannon Pawn Ranked Chariot with 7th Pawn Advancement vs. Screen Horse Defense with 7th Pawn "
        "Advancement and Right Ranked Chariot Variation",
        "", 8092271251648738946ULL, 12992132731240613109ULL),
    Eco("C34", -1,
        "Central Cannon Pawn Ranked Chariot with 7th Pawn Advancement vs. Screen Horse Defense with 7th Pawn "
        "Advancement and Right Pawn Ranked Cannon Variation",
        "", 12768230449704667035ULL, 475553040380788285ULL),
    Eco("C35", -1, "Central Cannon Pawn Ranked Chariot with 7th Pawn Advancement", "",
        1175918195263817080ULL, 4929161958332663127ULL),
    Eco("C36", -1,
        "Central Cannon Pawn Ranked Chariot with 7th Pawn Advancement and 7th Filed Horse vs. Screen Horse "
        "Defense with 7th Pawn Advancement and Left Riverbank Horse Variation",
        "", 8648296721267082081ULL, 13107562521594692696ULL),
    Eco("C37", -1,
        "Central Cannon Pawn Ranked Chariot with 7th Pawn Advancement and 7th Filed Horse vs. Screen Horse "
        "Defense with 7th Pawn Advancement and Left Riverbank Horse Right Central Elephant",
        "", 5649074234469967252ULL, 255354258954352164ULL),
    Eco("C38", -1,
        "Central Cannon Pawn Ranked Chariot with 7th Pawn Advancement and 7th Filed Horse with High Left "
        "Cannon vs. Screen Horse Defense with 7th Pawn Advancement and Left Riverbank Horse Right Central "
        "Elephant",
        "", 8834424694822827426ULL, 16478199320670708241ULL),
    Eco("C39", -1,
        "Central Cannon Pawn Ranked Chariot with 7th Pawn Advancement and 7th Filed Horse with Side Left "
        "Cannon vs. Screen Horse Defense with 7th Pawn Advancement and Left Riverbank Horse Right Central "
        "Elephant",
        "", 1729880779713760960ULL, 15215700232350546370ULL),
    Eco("C40", -1,
        "Central Cannon Pawn Ranked Chariot with 7th Pawn Advancement vs. Screen Horse Defense with 7th Pawn "
        "Advancement and Edge Cannon for Chariot Exchange Variation",
        "", 11699401650886995201ULL, 2782462563748799914ULL),
    Eco("C41", -1,
        "Central Cannon Pawn Ranked Chariot with 7th Pawn Advancement vs. Screen Horse Defense with 7th Pawn "
        "Advancement and Edge Cannon for Chariot Exchange Variation with side cannon retreating to throat "
        "rank",
        "", 2416282540297633941ULL, 9034519337530966019ULL),
    Eco("C42", -1,
        "Central Cannon Pawn Ranked Chariot with 7th Pawn Advancement 7th Filed Horse vs. Screen Horse "
        "Defense with 7th Pawn Advancement and Edge Cannon for Chariot Exchange Variation with side cannon "
        "retreating to throat rank",
        "", 5322414154151741068ULL, 10151539611080835340ULL),
    Eco("C43", -1, "Central Cannon Pawn Ranked Chariot with 7th Pawn Advancement 7th Filed Horse", "",
        16516769881850790421ULL, 2487129628337611585ULL),
    Eco("C44", -1,
        "Central Cannon Pawn Ranked Chariot with 7th Pawn Advancement Left Riverbank Horse vs. Screen Horse "
        "Defense with 7th Pawn Advancement and Edge Cannon for Chariot Exchange Variation with side cannon "
        "retreating to throat rank with right advisor subvariation",
        "", 10962274238685587770ULL, 16532834998500298929ULL),
    Eco("C45", -1,
        "Central Cannon Pawn Ranked Chariot with 7th Pawn Advancement Left Side Cannon vs. Screen Horse "
        "Defense with 7th Pawn Advancement and Edge Cannon for Chariot Exchange Variation with side cannon "
        "retreating to throat rank with right advisor subvariation",
        "", 12921694586852600129ULL, 17448698535505030311ULL),
    Eco("C46", -1,
        "Central Cannon Pawn Ranked Chariot with 7th Pawn Advancement Left Side Cannon vs. Screen Horse "
        "Defense with 7th Pawn Advancement and Edge Cannon for Chariot Exchange Variation with side cannon "
        "retreating to throat rank with right advisor and Right Filed Chariot subvariation",
        "", 803134643235836268ULL, 15784543551192818532ULL),
    Eco("C46", -1,
        "Central Cannon Pawn Ranked Chariot with 7th Pawn Advancement Left Side Cannon vs. Screen Horse "
        "Defense with 7th Pawn Advancement and Edge Cannon for Chariot Exchange Variation with side cannon "
        "retreating to throat rank with right advisor and Right Filed Chariot subvariation",
        "", 18181064747107532244ULL, 18278579489969111570ULL),
    Eco("C47", -1,
        "Central Cannon Pawn Ranked Chariot with 7th Pawn Advancement Left Side Horse vs. Screen Horse "
        "Defense with 7th Pawn Advancement and Edge Cannon for Chariot Exchange Variation with side cannon "
        "retreating to throat rank",
        "", 12373720183332329885ULL, 16961302367289709157ULL),
    Eco("C49", -1,
        "Central Cannon Pawn Ranked Chariot with 7th and 5th Pawn Advancement vs. Screen Horse Defense with "
        "7th Pawn Advancement and Edge Cannon for Chariot Exchange Variation with side cannon retreating to "
        "throat rank",
        "", 6057693142490908215ULL, 646385852179056289ULL),
    Eco("C52", -1,
        "56 Cannons Left side Horse vs. Screen Horse Defense with 7th Pawn Advancement and Right Filed "
        "Chariot",
        "", 14423578298197379293ULL, 14530207539554851128ULL),
    Eco("C53", -1,
        "56 Cannons Left side Horse vs. Screen Horse Defense with 7th Pawn Advancement Right Filed Chariot "
        "with Right Pawn Ranked Cannon",
        "", 2118987321198029803ULL, 9017984699298140944ULL),
    Eco("C54", -1, "56 Cannons Pawn Ranked Chariot vs. Screen Horse Defense", "", 13621329437894847184ULL,
        8656450098277220485ULL),
    Eco("C55", -1,
        "56 Cannons Pawn Ranked Chariot vs. Screen Horse Defense with 7th Pawn Advancement Right Filed "
        "Chariot",
        "", 10548230836810050310ULL, 14273599846013400765ULL),
    Eco("C56", -1,
        "56 Cannons Pawn Ranked Chariot vs. Screen Horse Defense with Double Headed Snake Pawn Formation", "",
        3563329945477511638ULL, 7153436170522405496ULL),
    Eco("C60", -1, "57 Cannons", "", 428510737931263956ULL, 10355763680543441629ULL),
    Eco("C72", -1,
        "57 Cannons 3rd Pawn Advancement vs. Screen Horse Defense with 1st and 3rd Pawn Advancement and "
        "Right Riverbank Horse Variation",
        "", 3186997729209507281ULL, 6322003560754863961ULL),
    Eco("C73", -1,
        "57 Cannons 3rd Pawn Advancement Left Ranked Chariot vs. Screen Horse Defense with 1st and 3rd Pawn "
        "Advancement and Right Riverbank Horse Variation",
        "", 16855929271218322688ULL, 16247639848940030156ULL),
    Eco("C74", -1,
        "57 Cannons 3rd Pawn Advancement Left Ranked Chariot vs. Screen Horse Defense with 1st and 3rd Pawn "
        "Advancement Right Riverbank Horse Variation with Left Central Elephant Subvariation",
        "", 6881521874038738812ULL, 15499790411234635321ULL),
    Eco("C75", -1,
        "57 Cannons 3rd Pawn Advancement Left Ranked Chariot Right Riverbank Horse vs. Screen Horse Defense "
        "with 1st and 3rd Pawn Advancement Right Riverbank Horse Variation with Left Central Elephant "
        "Subvariation",
        "", 10985529916803579020ULL, 12253407991972627734ULL),
    Eco("C76", -1,
        "57 Cannons 3rd Pawn Advancement Left Ranked Chariot and Right Riverbank Chariot vs. Screen Horse "
        "Defense with 1st and 3rd Pawn Advancement Right Riverbank Horse Variation with Left Central "
        "Elephant Subvariation",
        "", 15977929951232477563ULL, 16048969547154200109ULL),
    Eco("C77", -1,
        "57 Cannons 3rd Pawn Advancement Left Ranked Chariot vs. Screen Horse Defense with 1st and 3rd Pawn "
        "Advancement Right Riverbank Horse Variation with Right Central Elephant Subvariation",
        "", 16108581227459222517ULL, 6275607378355698352ULL),
    Eco("C78", -1,
        "57 Cannons 3rd Pawn Advancement Left Ranked Chariot vs. Screen Horse Defense with 1st and 3rd Pawn "
        "Advancement Right Riverbank Horse Variation with Side Pawn Exchange Subvariation",
        "", 15486679244918748076ULL, 14827719484274462795ULL),
    Eco("C85", -1, "Central Cannon with Riverbank Cannon Left Riverbank Horse", "", 7389623052098567998ULL,
        2869205249881542625ULL),
    Eco("C86", -1,
        "Central Cannon with Riverbank Cannon Deferred Chariot variation and Right ranked chariot "
        "subvariation vs. Screen Horse Defense with Left Riverbank Horse Variation",
        "", 8265108694832819473ULL, 2415722063873326849ULL),
    Eco("D00", -1, "Same Direction Cannons: Deferred Chariot vs. Others", "", 4742190871045813778ULL,
        11742420413237580304ULL),
    Eco("D01", -1, "Same Direction Cannons: Deferred Chariot vs. Ranked Chariot", "", 15486679244918748076ULL,
        14827719484274462795ULL),
    Eco("D02", -1, "Same Direction Cannons: Deferred Chariot vs. Filed Chariot", "", 16917542229639398552ULL,
        2536608519865025495ULL),
    Eco("D03", -1, "Same Direction Cannons: Ranked vs. Deferred Chariot", "", 2258772455648623250ULL,
        9904759696686446594ULL),
    Eco("D03", -1, "Same Direction Cannons: Ranked vs. Deferred Chariot", "", 17667301227101635010ULL,
        4703091882688041191ULL),
    Eco("D04", -1, "Same Direction Cannons: Ranked Chariot vs. Filed Chariot", "", 15854096447467638273ULL,
        17956669021238257866ULL),
    Eco("D05", -1,
        "Same Direction Cannons: Ranked Chariot vs. Filed Chariot with Riverbank chariot variation", "",
        15442525598235699797ULL, 5694487194187771260ULL),
    Eco("D05", -1,
        "Same Direction Cannons: Ranked Chariot vs. Filed Chariot with Riverbank chariot variation", "",
        16506704108071052166ULL, 11083674762036497147ULL),
    Eco("D10", -1, "Same Direction Cannons: Filed Chariot vs. Deferred Chariot", "", 10651507051003751359ULL,
        15346550107477218220ULL),
    Eco("D11", -1, "Same Direction Cannons: Filed Chariot vs. Deferred Chariot with left ranked chariot", "",
        10774556068684733279ULL, 13891143669864874371ULL),
    Eco("D11", -1, "Same Direction Cannons: Filed Chariot vs. Deferred Chariot with left ranked chariot", "",
        6752746292944221114ULL, 3076337711051602643ULL),
    Eco("D11", -1, "Same Direction Cannons: Filed Chariot vs. Deferred Chariot with left ranked chariot", "",
        16623992599609849787ULL, 13635807670906000266ULL),
    Eco("D11", -1, "Same Direction Cannons: Filed Chariot vs. Deferred Chariot with left ranked chariot", "",
        8189907773712027200ULL, 3035481535584234097ULL),
    Eco("D12", -1, "Same Direction Cannons: Filed Chariot vs. Deferred Chariot with Right ranked chariot", "",
        10774556068684733279ULL, 13891143669864874371ULL),
    Eco("D12", -1, "Same Direction Cannons: Filed Chariot vs. Deferred Chariot with Right ranked chariot", "",
        6752746292944221114ULL, 3076337711051602643ULL),
    Eco("D12", -1, "Same Direction Cannons: Filed Chariot vs. Deferred Chariot with Right ranked chariot", "",
        16623992599609849787ULL, 13635807670906000266ULL),
    Eco("D12", -1, "Same Direction Cannons: Filed Chariot vs. Deferred Chariot with Right ranked chariot", "",
        8189907773712027200ULL, 3035481535584234097ULL),
    Eco("D13", -1, "Same Direction Cannons: Filed Chariot vs. Deferred Chariot with chariot exchange offer",
        "", 8271553476471327897ULL, 9725986545991651537ULL),
    Eco("D13", -1, "Same Direction Cannons: Filed Chariot vs. Deferred Chariot with chariot exchange offer",
        "", 14540062467990063256ULL, 1257880017845090696ULL),
    Eco("D13", -1, "Same Direction Cannons: Filed Chariot vs. Deferred Chariot with chariot exchange offer",
        "", 6833228699315138915ULL, 9685219410567004275ULL),
    Eco("D14", -1,
        "Same Direction Cannons: Filed Chariot vs. Deferred Chariot with Pawn Ranked Cannon variation", "",
        10278135589460942956ULL, 9947330915301450452ULL),
    Eco("D14", -1,
        "Same Direction Cannons: Filed Chariot vs. Deferred Chariot with Pawn Ranked Cannon variation", "",
        3864543764436460653ULL, 2126053952573508493ULL),
    Eco("D14", -1,
        "Same Direction Cannons: Filed Chariot vs. Deferred Chariot with Pawn Ranked Cannon variation", "",
        11726725402340154774ULL, 9988260792338936438ULL),
    Eco("D15", -1, "Same Direction Cannons: Filed Chariot vs. Deferred Chariot with side cannon variation",
        "", 14944786268733952741ULL, 17915392837944500560ULL),
    Eco("D15", -1, "Same Direction Cannons: Filed Chariot vs. Deferred Chariot with side cannon variation",
        "", 8386916534390675172ULL, 8011064605054470153ULL),
    Eco("D15", -1, "Same Direction Cannons: Filed Chariot vs. Deferred Chariot with side cannon variation",
        "", 16390987928468434719ULL, 17884119967173800434ULL),
    Eco("D20", -1, "Same Direction Cannons: Filed Chariot", "", 10774556068684733279ULL,
        13891143669864874371ULL),
    Eco("D21", -1, "Same Direction Cannons: Filed Chariot with early advisor vs. Ranked Chariot", "",
        6873145185384698151ULL, 16987664354623857312ULL),
    Eco("D21", -1, "Same Direction Cannons: Filed Chariot with early advisor vs. Ranked Chariot", "",
        13727506580602517628ULL, 730701447125486587ULL),
    Eco("D22", -1, "Same Direction Cannons: Filed Chariot with Left Side Horse vs. Ranked Chariot", "",
        2285292523658805847ULL, 6252377590067336165ULL),
    Eco("D23", -1, "Same Direction Cannons: Filed Chariot with Riverbank Chariot vs. Ranked Chariot", "",
        1710233871122570584ULL, 14511851536757504407ULL),
    Eco("D23", -1, "Same Direction Cannons: Filed Chariot with Riverbank Chariot vs. Ranked Chariot", "",
        18290163700174757190ULL, 3551032868658462860ULL),
    Eco("D23", -1, "Same Direction Cannons: Filed Chariot with Riverbank Chariot vs. Ranked Chariot", "",
        16108113577762043325ULL, 2527202587305509575ULL),
    Eco("D24", -1, "Same Direction Cannons: Filed Chariot with Pawn Ranked Chariot vs. Ranked Chariot", "",
        18141085338896435413ULL, 6998379324696648349ULL),
    Eco("D24", -1, "Same Direction Cannons: Filed Chariot with Pawn Ranked Chariot vs. Ranked Chariot", "",
        3743175535942246448ULL, 10045601822910144973ULL),
    Eco("D25", -1, "Same Direction Cannons: Filed Chariot with Palcorner Cannon vs. Ranked Chariot", "",
        16354767925954068830ULL, 18231430853134069465ULL),
    Eco("D26", -1, "Same Direction Cannons: Filed Chariot with 3rd Pawn Advancement vs. Ranked Chariot", "",
        17682558781100439918ULL, 18001387746835262468ULL),
    Eco("D26", -1, "Same Direction Cannons: Filed Chariot with 3rd Pawn Advancement vs. Ranked Chariot", "",
        4419556081727541643ULL, 1416346413646174036ULL),
    Eco("D27", -1, "Same Direction Cannons: Filed Chariot with 7th Pawn Advancement vs. Ranked Chariot", "",
        12434560612437243608ULL, 11539552376347780018ULL),
    Eco("D27", -1, "Same Direction Cannons: Filed Chariot with 7th Pawn Advancement vs. Ranked Chariot", "",
        7250819244338683453ULL, 5355519892320770274ULL),
    Eco("D30", -1,
        "Central Cannon without 3rd Pawn Advancement vs. Left Cannon Blockade with Deferred Opposite "
        "Direction Cannon Variation",
        "", 15303378376170322266ULL, 4188716982913781775ULL),
    Eco("D32", -1,
        "Central Cannon with 3rd Pawn Advancement Right Riverbank Horse vs. Left Cannon Blockade with "
        "Deferred Opposite Direction Cannon Variation",
        "", 1411006996998425258ULL, 5131789118206622496ULL),
    Eco("D33", -1,
        "Central Cannon with 3rd Pawn Advancement 7th File Horse vs. Left Cannon Blockade with Deferred "
        "Opposite Direction Cannon Variation",
        "", 13561986505044672323ULL, 14673224440592027904ULL),
    Eco("D34", -1,
        "Central Cannon with 3rd Pawn Advancement Lt Side Horse vs. Left Cannon Blockade with Deferred "
        "Opposite Direction Cannon Variation",
        "", 6796091631187992658ULL, 12403729469651354217ULL),
    Eco("D35", -1,
        "Central Cannon with 3rd Pawn Advancement Cannon attacking horse variation vs. Left Cannon Blockade "
        "with Deferred Opposite Direction Cannon Variation",
        "", 113889193730533172ULL, 16582928227075566079ULL),
    Eco("D36", -1,
        "Central Cannon 3rd Pawn Advancement with Double-headed snake pawn formation vs.Left Cannon Blockade "
        "with Deferred Opposite Direction Cannon Variation",
        "", 17110866974316926173ULL, 6539991686204355134ULL),
    Eco("D50", -1, "Opposite Direction Cannons", "", 18101424913960747821ULL, 1733884017462355759ULL),
    Eco("D51", -1, "Central Cannon", "", 1243898692237359229ULL, 15005858729502379978ULL),
    Eco("D51", -1, "Central Cannon", "", 7694171952452268209ULL, 7301127831332843851ULL),
    Eco("D51", -1, "Central Cannon", "", 17684997297349995874ULL, 4240410407708664888ULL),
    Eco("E00", -1, "Pawn vs. Others", "", 6994019504289044180ULL, 4105806037407979874ULL),
    Eco("E01", -1, "Pawn", "", 6300211200527790113ULL, 10274567982782884638ULL),
    Eco("E01", -1, "Pawn", "", 15520502024523621544ULL, 1052619434557144983ULL),
    Eco("E02", -1, "Pawn with Rt filed Horse vs. Elephant Opening", "", 12028644774833353006ULL,
        16628352463127358727ULL),
    Eco("E02", -1, "Pawn with Rt filed Horse vs. Elephant Opening", "", 2802200327923672487ULL,
        7408131795341932942ULL),
    Eco("E03", -1, "Pawn vs. Central Cannon", "", 14180177214564931058ULL, 2865951852236468091ULL),
    Eco("E03", -1, "Pawn vs. Central Cannon", "", 9092934219092156621ULL, 11330884535193508420ULL),
    Eco("E04", -1, "Pawn vs. Palcorner Cannon or Cross Palace Cannon", "", 12311827981535613091ULL,
        17683949177231667276ULL),
    Eco("E04", -1, "Pawn vs. Palcorner Cannon or Cross Palace Cannon", "", 12438798606279016442ULL,
        17521563493410742037ULL),
    Eco("E04", -1, "Pawn vs. Palcorner Cannon or Cross Palace Cannon", "", 1613966628031171269ULL,
        5319756512698264106ULL),
    Eco("E04", -1, "Pawn vs. Palcorner Cannon or Cross Palace Cannon", "", 1163878294473762204ULL,
        5733233372498756979ULL),
    Eco("E05", -1, "Pawn vs. Golden Hooked Cannon", "", 14424419379755058571ULL, 4099291042004322960ULL),
    Eco("E06", -1, "Pawn", "", 13937123167951845416ULL, 2525487616432464701ULL),
    Eco("E07", -1, "Pawn with Right horse Advancement", "", 3524655669440117031ULL, 5429382684458116388ULL),
    Eco("E08", -1, "Double-headed Snake Pawn Formation vs. Right Horse Advancement", "",
        17905218085510174127ULL, 4895322316014469388ULL),
    Eco("E09", -1,
        "Double-headed Snake Pawn Formation vs. Right Horse Advancement with Thundering Defense Variation",
        "", 16979974812744643535ULL, 17945856522639976385ULL),
    Eco("E10", -1, "Pawn", "", 8275186699425804468ULL, 9374541754975109039ULL),
    Eco("E11", -1, "Pawn with Left or Right Elephant vs. Thundering Defense", "", 14737513259515911370ULL,
        11135067506066175443ULL),
    Eco("E11", -1, "Pawn with Left or Right Elephant vs. Thundering Defense", "", 7658928196775443144ULL,
        4343832072043516881ULL),
    Eco("E12", -1, "Pawn with Right Central Cannon vs. Thundering Defense", "", 10894523231380894053ULL,
        4485315264547499843ULL),
    Eco("E13", -1, "Pawn with Left Central Cannon vs. Thundering Defense", "", 14911774622367735896ULL,
        7493797722221624958ULL),
    Eco("E14", -1, "Pawn with Left Central Cannon vs. Thundering Defense with Right Central Elephant", "",
        17910854309554276013ULL, 15101423466468723714ULL),
    Eco("E15", -1,
        "Pawn with Left Central Cannon Right Side Horse Variation vs. Thundering Defense with Right Central "
        "Elephant",
        "", 7967864733177222347ULL, 6603152782514738442ULL),
    Eco("E16", -1,
        "Pawn with Left Central Cannon Right Side Horse Variation vs. Thundering Defense with Right Central "
        "Elephant with Left Side Horse Variation",
        "", 2265469250727456392ULL, 13485346666293307585ULL),
    Eco("E17", -1,
        "Pawn with Left Central Cannon vs. Thundering Defense with Same Direction Cannons Variation", "",
        15118140654223349313ULL, 13995765930463919448ULL),
    Eco("E20", -1, "Pawn with Left Central Cannon", "", 8691135779014087204ULL, 5881197850612635787ULL),
    Eco("E21", -1,
        "Pawn with Left Central Cannon and Early Advisor Variation vs. Thundering Defense with Left Central "
        "Elephant",
        "", 12860492147830528092ULL, 8834155234471410600ULL),
    Eco("E22", -1,
        "Pawn with Left Central Cannon and Left File Horse Variation vs. Thundering Defense with Left "
        "Central Elephant",
        "", 1209289771352855613ULL, 11537864134433905028ULL),
    Eco("E23", -1, "Pawn with Left Central Cannon and Left File Horse Variation", "", 357725219204351506ULL,
        11982269769273911652ULL),
    Eco("E24", -1, "Pawn with Left Central Cannon and Left Filed Chariot Variation", "",
        13757001770219720019ULL, 1524933591951408599ULL),
    Eco("E25", -1,
        "Pawn with Left Central Cannon and Left Filed Chariot Variation and Right Riverbank Horse vs. "
        "Thundering Defense with Left Central Elephant and Right Ranked Chariot and Early Advisor Variation",
        "", 16672295402536480451ULL, 16380193135690351478ULL),
    Eco("E26", -1,
        "Pawn with Left Central Cannon and Left Filed Chariot Variation and Right Riverbank Horse vs. "
        "Thundering Defense with Left Central Elephant and Right Ranked Chariot and Early Advisor Variation",
        "", 4683709480540008947ULL, 8794010423051073607ULL),
    Eco("E27", -1,
        "Pawn with Left Central Cannon and Left Filed Chariot Variation and Right Riverbank Horse vs. "
        "Thundering Defense with Left Central Elephant and Right Ranked Chariot and 1st Pawn Advancement",
        "", 5446887966385123783ULL, 11755573606941502685ULL),
    Eco("E30", -1,
        "Pawn with Left Central Cannon vs. Thundering Defense with Left Central Elephant and 7th Pawn "
        "Advancement",
        "", 11291529096635991867ULL, 13508905003522831225ULL),
    Eco("E31", -1, "Pawn with Left Central Cannon", "", 8441199718554371846ULL, 6209712580281669783ULL),
    Eco("E31", -1, "Pawn with Left Central Cannon", "", 6708793086017584185ULL, 16462131512226326370ULL),
    Eco("E32", -1,
        "Pawn with Left Central Cannon Left Filed Chariot and Right Side Horse vs. Thundering Defense with "
        "Left Central Elephant and 7th Pawn Advancement Twice and Lame Horse variation",
        "", 4636197952870637824ULL, 10728560162223691144ULL),
    Eco("E32", -1,
        "Pawn with Left Central Cannon Left Filed Chariot and Right Side Horse vs. Thundering Defense with "
        "Left Central Elephant and 7th Pawn Advancement Twice and Lame Horse variation",
        "", 4636197952870637824ULL, 10728560162223691144ULL),
    Eco("E33", -1, "Pawn with Left Central Cannon Left Filed Chariot and Right Side Horse", "",
        16079932653261208688ULL, 7499057601929023114ULL),
    Eco("E33", -1, "Pawn with Left Central Cannon Left Filed Chariot and Right Side Horse", "",
        16079932653261208688ULL, 7499057601929023114ULL),
    Eco("E34", -1,
        "Pawn with Left Central Cannon Left Filed Chariot and Right Side Horse with Early Advisor vs. "
        "Thundering Defense with Left Central Elephant and 7th Pawn Advancement Twice and Right Ranked "
        "Chariot",
        "", 1568340106369545736ULL, 4833785168817941929ULL),
    Eco("E34", -1,
        "Pawn with Left Central Cannon Left Filed Chariot and Right Side Horse with Early Advisor vs. "
        "Thundering Defense with Left Central Elephant and 7th Pawn Advancement Twice and Right Ranked "
        "Chariot",
        "", 1568340106369545736ULL, 4833785168817941929ULL),
    Eco("E35", -1,
        "Pawn with Left Central Cannon Riverbank Chariot and Right Side Horse vs. Thundering Defense with "
        "Left Central Elephant and 7th Pawn Advancement Twice and Right Ranked Chariot",
        "", 15457567823953642596ULL, 16874724083308005517ULL),
    Eco("E35", -1,
        "Pawn with Left Central Cannon Riverbank Chariot and Right Side Horse vs. Thundering Defense with "
        "Left Central Elephant and 7th Pawn Advancement Twice and Right Ranked Chariot",
        "", 15457567823953642596ULL, 16874724083308005517ULL),
    Eco("E36", -1,
        "Pawn with Left Central Cannon Double Filed Chariot and Right Side Horse vs. Thundering Defense with "
        "Left Central Elephant and 7th Pawn Advancement Twice and Right Ranked Chariot",
        "", 16667567714957662861ULL, 15433529557137418195ULL),
    Eco("E36", -1,
        "Pawn with Left Central Cannon Double Filed Chariot and Right Side Horse vs. Thundering Defense with "
        "Left Central Elephant and 7th Pawn Advancement Twice and Right Ranked Chariot",
        "", 16667567714957662861ULL, 15433529557137418195ULL),
    Eco("E37", -1,
        "Pawn with Left Central Cannon Right Side Horse vs. Thundering Defense with Left Central Elephant",
        "", 17194378522336378946ULL, 15829627745169506691ULL),
    Eco("E38", -1,
        "Pawn with Left Central Cannon with Cannon taking Central Pawn Variation vs. Thundering Defense with "
        "Left Central Elephant",
        "", 11401306062007325912ULL, 13203068445650345591ULL),
    Eco("E40", -1, "Pawn vs. Pawn", "", 8820370673304337449ULL, 13006698737473587812ULL),
    Eco("E41", -1, "Pawn with Right Horse Variation vs. Pawn", "", 10081532482546203942ULL,
        15912427790477036669ULL),
    Eco("E42", -1, "Pawn with Right Horse Variation", "", 3138420031077340122ULL, 14349270547927512610ULL),
    Eco("E42", -1, "Pawn with Right Horse Variation", "", 12080937629905405317ULL, 3623168688273247684ULL),
    Eco("E43", -1,
        "Pawn with Right Horse Left/Right Central Elephant Variation vs. Pawn with Right Horse Variation", "",
        3680369483330869670ULL, 8751529567437478492ULL),
    Eco("E43", -1,
        "Pawn with Right Horse Left/Right Central Elephant Variation vs. Pawn with Right Horse Variation", "",
        10798677720982372260ULL, 16121898625796914270ULL),
    Eco("E43", -1,
        "Pawn with Right Horse Left/Right Central Elephant Variation vs. Pawn with Right Horse Variation", "",
        3680369483330869670ULL, 8751529567437478492ULL),
    Eco("E43", -1,
        "Pawn with Right Horse Left/Right Central Elephant Variation vs. Pawn with Right Horse Variation", "",
        10798677720982372260ULL, 16121898625796914270ULL),
    Eco("E44", -1, "Pawn with Right Horse Right Ranked Chariot Variation vs. Pawn with Right Horse Variation",
        "", 8438207001854713690ULL, 17051590090502577200ULL),
    Eco("E44", -1, "Pawn with Right Horse Right Ranked Chariot Variation vs. Pawn with Right Horse Variation",
        "", 8438207001854713690ULL, 17051590090502577200ULL),
    Eco("E45", -1, "Pawn with Right Horse Right Side Cannon Variation vs. Pawn with Right Horse Variation",
        "", 18099928991306325052ULL, 10468221478746201462ULL),
    Eco("E45", -1, "Pawn with Right Horse Right Side Cannon Variation vs. Pawn with Right Horse Variation",
        "", 18099928991306325052ULL, 10468221478746201462ULL),
    Eco("E46", -1, "Pawn with Thundering Cannon", "", 9112585089967249176ULL, 4240776927164496988ULL),
    Eco("E47", -1, "Pawn with Thundering Cannon", "", 7010221207378387201ULL, 11466137715871432570ULL),
    Eco("E48", -1, "Pawn with Thundering Cannon", "", 15831213164045964350ULL, 2730708550958041669ULL),
};

static auto& ecoXqMap() // built on first use, not at load time (clazy non-pod-global-static, 2026-11-15)
{
    static std::map<u64, int> v;
    return v;
}
static auto& ecoXqNameSet() // built on first use, not at load time (clazy non-pod-global-static, 2026-11-15)
{
    static std::set<std::string> v;
    return v;
}

/// The tables are built on first use, which can come from several threads at once (an
/// opening tree reads games on a pool): once, under call_once. Two threads filling the
/// map together corrupted it and gave wrong openings
static std::once_flag ecoXqInitFlag;
static void ecoXqInit()
{
    std::call_once(ecoXqInitFlag, [] {
        for (size_t i = 0; i < ecoXqVec.size(); ++i) {
            auto e = ecoXqVec[i];
            ecoXqMap()[e.hash] = i;
            if (e.flipHash > 0) {
                ecoXqMap()[e.flipHash] = i;
            }
            ecoXqNameSet().insert(e.ecoString);
        }
    });
}

Eco XqBoard::getEco(u64 key) const
{
    ecoXqInit();

    auto it = ecoXqMap().find(key);
    if (it != ecoXqMap().end()) {
        auto r = it->second;
        if (static_cast<size_t>(r) < ecoXqVec.size()) return ecoXqVec.at(r);
    }
    return Eco();
}

bool XqBoard::isEcoName(const std::string name) const
{
    ecoXqInit();
    return ecoXqNameSet().find(name) != ecoXqNameSet().end();
}
