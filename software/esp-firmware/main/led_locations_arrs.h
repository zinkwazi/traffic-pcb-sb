/**
 * led_locations_arrs.h
 * 
 * This file is an extension file to 
 * led_locations.h meant to aid readability.
 */

#ifndef LED_LOCATIONS_ARRS_H_
#define LED_LOCATIONS_ARRS_H_

#include <stdint.h>

struct LEDLoc {
    const int16_t *hardwareNums;
    const float latitude;
    const float longitude;
    const int flowSpeed;
};

typedef struct LEDLoc LEDLoc;

/**
 * These arrays are structured as follows:
 * They are an array of hardware led numbers
 * which correspond to the same physical coordinates.
 * When the first element is positive, then there should
 * be only one element and it is the hardware number of
 * the only LED of one coordinate. When the first element
 * is negative, -ele is the number of hardware numbers
 * to follow. ie. the form is one of the following:
 * {hardwareNum1}
 * {-length, hardwareNum1, hardwareNum2, ...}
 * 
 * This form saves one element per array when there is only
 * a single element and does not increase size when there
 * are multiple. int16_t is used over uint16_t because both
 * can store the maximum hardware number, which is below 400
 * but above 255.
 */
static const int16_t south1[] = {1};
static const int16_t south2[] = {-2, 2, 3};
static const int16_t south4[] = {-2, 4, 5};
static const int16_t south6[] = {6};
static const int16_t south7[] = {7};
static const int16_t south8[] = {8};
static const int16_t south9[] = {9};
static const int16_t south10[] = {10};
static const int16_t south11[] = {11};
static const int16_t south12[] = {12};
static const int16_t south13[] = {13};
static const int16_t south14[] = {14};
static const int16_t south15[] = {15};
static const int16_t south16[] = {16};
static const int16_t south17[] = {17};
static const int16_t south18[] = {18};
static const int16_t south19[] = {-2, 19, 20};
static const int16_t south21[] = {-2, 21, 22};
static const int16_t south23[] = {23};
static const int16_t south24[] = {24};
static const int16_t south25[] = {-2, 25, 26};
static const int16_t south27[] = {-7, 27, 28, 29, 30, 31, 32, 33};
static const int16_t south34[] = {34};
static const int16_t south35[] = {35};
static const int16_t south36[] = {36};
static const int16_t south37[] = {37};
static const int16_t south38[] = {38};
static const int16_t south39[] = {39};
static const int16_t south40[] = {40};
static const int16_t south41[] = {41};
static const int16_t south42[] = {42};
static const int16_t south43[] = {43};
static const int16_t south44[] = {44};
static const int16_t south45[] = {45};
static const int16_t south46[] = {46};
static const int16_t south47[] = {47};
static const int16_t south48[] = {48};
static const int16_t south49[] = {49};
static const int16_t south50[] = {50};
static const int16_t south51[] = {51};
static const int16_t south52[] = {52};
static const int16_t south53[] = {53};
static const int16_t south54[] = {54};
static const int16_t south55[] = {55};
static const int16_t south56[] = {56};
static const int16_t south57[] = {57};
static const int16_t south58[] = {58};
static const int16_t south59[] = {59};
static const int16_t south60[] = {60};
static const int16_t south61[] = {-4, 61, 62, 63, 64};
static const int16_t south65[] = {65};
static const int16_t south66[] = {66};
static const int16_t south67[] = {67};
static const int16_t south68[] = {68};
static const int16_t south69[] = {69};
static const int16_t south70[] = {-3, 70, 71, 72};
static const int16_t south73[] = {73};
static const int16_t south74[] = {74};
static const int16_t south75[] = {75};
static const int16_t south76[] = {-2, 76, 77};
static const int16_t south78[] = {78};
static const int16_t south79[] = {79};
static const int16_t south80[] = {80};
static const int16_t south81[] = {-4, 81, 82, 83, 84};
static const int16_t south85[] = {-2, 85, 86};
static const int16_t south87[] = {-2, 87, 88};
static const int16_t south89[] = {-4, 89, 90, 109, 110};
static const int16_t south91[] = {91};
static const int16_t south92[] = {92};
static const int16_t south93[] = {93};
static const int16_t south94[] = {94};
static const int16_t south95[] = {95};
static const int16_t south96[] = {-2, 96, 97};
static const int16_t south98[] = {98};
static const int16_t south99[] = {99};
static const int16_t south100[] = {100};
static const int16_t south101[] = {101};
static const int16_t south102[] = {102};
static const int16_t south103[] = {103};
static const int16_t south104[] = {104};
static const int16_t south105[] = {105};
static const int16_t south106[] = {106};
static const int16_t south107[] = {-2, 107, 108};
static const int16_t south111[] = {111};
static const int16_t south112[] = {-2, 112, 113};
static const int16_t south114[] = {-2, 114, 115};
static const int16_t south116[] = {-3, 116, 117, 154};
static const int16_t south118[] = {-4, 118, 119, 120, 121};
static const int16_t south122[] = {122};
static const int16_t south123[] = {-2, 123, 124};
static const int16_t south124[] = {124};
static const int16_t south125[] = {125};
static const int16_t south126[] = {126};
static const int16_t south127[] = {127};
static const int16_t south128[] = {128};
static const int16_t south129[] = {129};
static const int16_t south130[] = {130};
static const int16_t south131[] = {-2, 131, 132};
static const int16_t south133[] = {133};
static const int16_t south134[] = {134};
static const int16_t south135[] = {135};
static const int16_t south136[] = {136};
static const int16_t south137[] = {137};
static const int16_t south138[] = {-2, 138, 139};
static const int16_t south140[] = {140};
static const int16_t south141[] = {141};
static const int16_t south142[] = {142};
static const int16_t south143[] = {143};
static const int16_t south144[] = {144};
static const int16_t south145[] = {145};
static const int16_t south146[] = {146};
static const int16_t south147[] = {147};
static const int16_t south148[] = {148};
static const int16_t south149[] = {149};
static const int16_t south150[] = {150};
static const int16_t south151[] = {151};
static const int16_t south152[] = {152};
static const int16_t south153[] = {153};
static const int16_t south155[] = {-4, 155, 156, 157, 158};
static const int16_t south159[] = {159};
static const int16_t south160[] = {160};
static const int16_t south161[] = {161};
static const int16_t south162[] = {162};
static const int16_t south163[] = {163};
static const int16_t south164[] = {164};
static const int16_t south165[] = {165};
static const int16_t south166[] = {-2, 166, 167};
static const int16_t south168[] = {168};
static const int16_t south169[] = {-2, 169, 170};
static const int16_t south171[] = {-2, 171, 253};
static const int16_t south172[] = {172};
static const int16_t south173[] = {173};
static const int16_t south174[] = {174};
static const int16_t south175[] = {175};
static const int16_t south176[] = {176};
static const int16_t south177[] = {177};
static const int16_t south178[] = {178};
static const int16_t south179[] = {179};
static const int16_t south180[] = {180};
static const int16_t south181[] = {181};
static const int16_t south182[] = {182};
static const int16_t south183[] = {183};
static const int16_t south184[] = {184};
static const int16_t south185[] = {185};
static const int16_t south186[] = {186};
static const int16_t south187[] = {187};
static const int16_t south188[] = {-2, 188, 189};
static const int16_t south190[] = {190};
static const int16_t south191[] = {191};
static const int16_t south192[] = {-2, 192, 193};
static const int16_t south194[] = {194};
static const int16_t south195[] = {195};
static const int16_t south196[] = {196};
static const int16_t south197[] = {197};
static const int16_t south198[] = {198};
static const int16_t south199[] = {199};
static const int16_t south200[] = {200};
static const int16_t south201[] = {201};
static const int16_t south202[] = {202};
static const int16_t south203[] = {203};
static const int16_t south204[] = {204};
static const int16_t south205[] = {205};
static const int16_t south206[] = {206};
static const int16_t south207[] = {-2, 207, 226};
static const int16_t south208[] = {208};
static const int16_t south209[] = {209};
static const int16_t south210[] = {210};
static const int16_t south211[] = {211};
static const int16_t south212[] = {212};
static const int16_t south213[] = {213};
static const int16_t south214[] = {214};
static const int16_t south215[] = {215};
static const int16_t south216[] = {216};
static const int16_t south217[] = {217};
static const int16_t south218[] = {218};
static const int16_t south219[] = {219};
static const int16_t south220[] = {220};
static const int16_t south221[] = {221};
static const int16_t south222[] = {222};
static const int16_t south223[] = {223};
static const int16_t south224[] = {224};
static const int16_t south225[] = {225};
static const int16_t south227[] = {227};
static const int16_t south228[] = {228};
static const int16_t south229[] = {229};
static const int16_t south230[] = {230};
static const int16_t south231[] = {231};
static const int16_t south232[] = {232};
static const int16_t south233[] = {-2, 233, 234};
static const int16_t south235[] = {235};
static const int16_t south236[] = {236};
static const int16_t south237[] = {237};
static const int16_t south238[] = {238};
static const int16_t south239[] = {239};
static const int16_t south240[] = {240};
static const int16_t south241[] = {241};
static const int16_t south242[] = {242};
static const int16_t south243[] = {243};
static const int16_t south244[] = {244};
static const int16_t south245[] = {245};
static const int16_t south246[] = {246};
static const int16_t south247[] = {247};
static const int16_t south248[] = {248};
static const int16_t south249[] = {249};
static const int16_t south250[] = {250};
static const int16_t south251[] = {251};
static const int16_t south252[] = {252};
static const int16_t south254[] = {254};
static const int16_t south255[] = {255};
static const int16_t south256[] = {256};
static const int16_t south257[] = {257};
static const int16_t south258[] = {258};
static const int16_t south259[] = {259};
static const int16_t south260[] = {260};
static const int16_t south261[] = {261};
static const int16_t south262[] = {262};
static const int16_t south263[] = {263};
static const int16_t south264[] = {264};
static const int16_t south265[] = {265};
static const int16_t south266[] = {266};
static const int16_t south267[] = {267};
static const int16_t south268[] = {268};
static const int16_t south269[] = {269};
static const int16_t south270[] = {270};
static const int16_t south271[] = {271};
static const int16_t south272[] = {272};
static const int16_t south273[] = {273};
static const int16_t south274[] = {274};
static const int16_t south275[] = {275};
static const int16_t south276[] = {276};
static const int16_t south277[] = {277};
static const int16_t south278[] = {278};
static const int16_t south279[] = {279};
static const int16_t south280[] = {280};
static const int16_t south281[] = {281};
static const int16_t south282[] = {282};
static const int16_t south283[] = {283};
static const int16_t south284[] = {284};
static const int16_t south285[] = {285};
static const int16_t south286[] = {286};
static const int16_t south287[] = {287};
static const int16_t south288[] = {288};
static const int16_t south289[] = {289};
static const int16_t south290[] = {290};
static const int16_t south291[] = {-2, 291, 292};
static const int16_t south293[] = {293};
static const int16_t south294[] = {294};
static const int16_t south295[] = {295};
static const int16_t south296[] = {296};
static const int16_t south297[] = {297};
static const int16_t south298[] = {298};
static const int16_t south299[] = {299};
static const int16_t south300[] = {300};
static const int16_t south301[] = {301};
static const int16_t south302[] = {302};
static const int16_t south303[] = {303};
static const int16_t south304[] = {304};
static const int16_t south305[] = {305};
static const int16_t south306[] = {306};
static const int16_t south307[] = {307};
static const int16_t south308[] = {308};
static const int16_t south309[] = {309};
static const int16_t south310[] = {310};
static const int16_t south311[] = {311};
static const int16_t south312[] = {312};
static const int16_t south313[] = {313};
static const int16_t south314[] = {314};
static const int16_t south315[] = {315};
static const int16_t south316[] = {316};
static const int16_t south317[] = {317};
static const int16_t south318[] = {318};
static const int16_t south319[] = {319};
static const int16_t south320[] = {-2, 320, 321};
static const int16_t south322[] = {-2, 322, 323};
static const int16_t south324[] = {324};
static const int16_t south329[] = {-2, 329, 330};


static const int16_t north1[] = {1};
static const int16_t north2[] = {-2, 2, 3};
static const int16_t north4[] = {-2, 4, 5};
static const int16_t north6[] = {6};
static const int16_t north7[] = {7};
static const int16_t north8[] = {8};
static const int16_t north9[] = {9};
static const int16_t north10[] = {10};
static const int16_t north11[] = {11};
static const int16_t north12[] = {12};
static const int16_t north13[] = {13};
static const int16_t north14[] = {14};
static const int16_t north15[] = {15};
static const int16_t north16[] = {16};
static const int16_t north17[] = {17};
static const int16_t north18[] = {18};
static const int16_t north19[] = {19};
static const int16_t north20[] = {-2, 20, 21};
static const int16_t north22[] = {22};
static const int16_t north23[] = {23};
static const int16_t north24[] = {24};
static const int16_t north25[] = {-8, 25, 26, 27, 28, 29, 30, 31, 32};
static const int16_t north33[] = {33};
static const int16_t north34[] = {-2, 34, 35};
static const int16_t north36[] = {36};
static const int16_t north37[] = {-2, 37, 38};
static const int16_t north39[] = {39};
static const int16_t north40[] = {40};
static const int16_t north41[] = {41};
static const int16_t north42[] = {42};
static const int16_t north43[] = {43};
static const int16_t north44[] = {44};
static const int16_t north45[] = {45};
static const int16_t north46[] = {46};
static const int16_t north47[] = {47};
static const int16_t north48[] = {48};
static const int16_t north49[] = {49};
static const int16_t north50[] = {50};
static const int16_t north51[] = {51};
static const int16_t north52[] = {52};
static const int16_t north53[] = {53};
static const int16_t north54[] = {54};
static const int16_t north55[] = {55};
static const int16_t north56[] = {56};
static const int16_t north57[] = {57};
static const int16_t north58[] = {58};
static const int16_t north59[] = {59};
static const int16_t north60[] = {60};
static const int16_t north61[] = {-4, 61, 62, 63, 64};
static const int16_t north65[] = {65};
static const int16_t north66[] = {66};
static const int16_t north67[] = {67};
static const int16_t north68[] = {68};
static const int16_t north69[] = {69};
static const int16_t north70[] = {-2, 70, 71};
static const int16_t north72[] = {72};
static const int16_t north73[] = {-2, 73, 74};
static const int16_t north75[] = {75};
static const int16_t north76[] = {-2, 76, 77};
static const int16_t north78[] = {78};
static const int16_t north79[] = {79};
static const int16_t north80[] = {80};
static const int16_t north81[] = {-4, 81, 82, 83, 84};
static const int16_t north85[] = {-2, 85, 86};
static const int16_t north87[] = {-2, 87, 88};
static const int16_t north89[] = {-4, 89, 90, 109, 110};
static const int16_t north91[] = {91};
static const int16_t north92[] = {92};
static const int16_t north93[] = {93};
static const int16_t north94[] = {94};
static const int16_t north95[] = {95};
static const int16_t north96[] = {-2, 96, 97};
static const int16_t north98[] = {98};
static const int16_t north99[] = {-2, 99, 100};
static const int16_t north101[] = {101};
static const int16_t north102[] = {102};
static const int16_t north103[] = {103};
static const int16_t north104[] = {104};
static const int16_t north105[] = {105};
static const int16_t north106[] = {106};
static const int16_t north107[] = {-2, 107, 108};
static const int16_t north111[] = {111};
static const int16_t north112[] = {-2, 112, 113};
static const int16_t north114[] = {-2, 114, 115};
static const int16_t north116[] = {-3, 116, 117, 154};
static const int16_t north118[] = {-4, 118, 119, 120, 121};
static const int16_t north122[] = {-3, 122, 123, 124};
static const int16_t north125[] = {125};
static const int16_t north126[] = {126};
static const int16_t north127[] = {127};
static const int16_t north128[] = {128};
static const int16_t north129[] = {129};
static const int16_t north130[] = {130};
static const int16_t north131[] = {-2, 131, 132};
static const int16_t north133[] = {133};
static const int16_t north134[] = {134};
static const int16_t north135[] = {135};
static const int16_t north136[] = {136};
static const int16_t north137[] = {137};
static const int16_t north138[] = {138};
static const int16_t north139[] = {139};
static const int16_t north140[] = {140};
static const int16_t north141[] = {141};
static const int16_t north142[] = {142};
static const int16_t north143[] = {143};
static const int16_t north144[] = {144};
static const int16_t north145[] = {145};
static const int16_t north146[] = {-4, 146, 147, 148, 149};
static const int16_t north150[] = {150};
static const int16_t north151[] = {151};
static const int16_t north152[] = {152};
static const int16_t north153[] = {153};
static const int16_t north155[] = {-3, 155, 156, 157};
static const int16_t north158[] = {158};
static const int16_t north159[] = {159};
static const int16_t north160[] = {160};
static const int16_t north161[] = {161};
static const int16_t north162[] = {162};
static const int16_t north163[] = {163};
static const int16_t north164[] = {164};
static const int16_t north165[] = {165};
static const int16_t north166[] = {166};
static const int16_t north167[] = {167};
static const int16_t north168[] = {168};
static const int16_t north169[] = {169};
static const int16_t north170[] = {-4, 170, 171, 253, 254};
static const int16_t north172[] = {172};
static const int16_t north173[] = {173};
static const int16_t north174[] = {174};
static const int16_t north175[] = {175};
static const int16_t north176[] = {176};
static const int16_t north177[] = {177};
static const int16_t north178[] = {178};
static const int16_t north179[] = {179};
static const int16_t north180[] = {180};
static const int16_t north181[] = {181};
static const int16_t north182[] = {182};
static const int16_t north183[] = {183};
static const int16_t north184[] = {184};
static const int16_t north185[] = {-2, 185, 186};
static const int16_t north187[] = {187};
static const int16_t north188[] = {188};
static const int16_t north189[] = {189};
static const int16_t north190[] = {190};
static const int16_t north191[] = {191};
static const int16_t north192[] = {192};
static const int16_t north193[] = {193};
static const int16_t north194[] = {194};
static const int16_t north195[] = {195};
static const int16_t north196[] = {196};
static const int16_t north197[] = {197};
static const int16_t north198[] = {198};
static const int16_t north199[] = {199};
static const int16_t north200[] = {200};
static const int16_t north201[] = {201};
static const int16_t north202[] = {202};
static const int16_t north203[] = {203};
static const int16_t north204[] = {204};
static const int16_t north205[] = {205};
static const int16_t north206[] = {206};
static const int16_t north207[] = {-2, 207, 226};
static const int16_t north208[] = {208};
static const int16_t north209[] = {209};
static const int16_t north210[] = {210};
static const int16_t north211[] = {211};
static const int16_t north212[] = {212};
static const int16_t north213[] = {213};
static const int16_t north214[] = {214};
static const int16_t north215[] = {215};
static const int16_t north216[] = {216};
static const int16_t north217[] = {217};
static const int16_t north218[] = {218};
static const int16_t north219[] = {-2, 219, 220};
static const int16_t north221[] = {221};
static const int16_t north222[] = {222};
static const int16_t north223[] = {223};
static const int16_t north224[] = {224};
static const int16_t north225[] = {225};
static const int16_t north227[] = {227};
static const int16_t north228[] = {228};
static const int16_t north229[] = {229};
static const int16_t north230[] = {230};
static const int16_t north231[] = {231};
static const int16_t north232[] = {232};
static const int16_t north233[] = {-2, 233, 234};
static const int16_t north235[] = {-2, 235, 236};
static const int16_t north237[] = {237};
static const int16_t north238[] = {-2, 238, 239};
static const int16_t north240[] = {240};
static const int16_t north241[] = {241};
static const int16_t north242[] = {242};
static const int16_t north243[] = {243};
static const int16_t north244[] = {244};
static const int16_t north245[] = {245};
static const int16_t north246[] = {246};
static const int16_t north247[] = {247};
static const int16_t north248[] = {248};
static const int16_t north249[] = {249};
static const int16_t north250[] = {-3, 250, 251, 252};
static const int16_t north255[] = {255};
static const int16_t north256[] = {256};
static const int16_t north257[] = {-2, 257, 258};
static const int16_t north259[] = {259};
static const int16_t north260[] = {260};
static const int16_t north261[] = {261};
static const int16_t north262[] = {262};
static const int16_t north263[] = {263};
static const int16_t north264[] = {264};
static const int16_t north265[] = {265};
static const int16_t north266[] = {266};
static const int16_t north267[] = {267};
static const int16_t north268[] = {268};
static const int16_t north269[] = {269};
static const int16_t north270[] = {270};
static const int16_t north271[] = {271};
static const int16_t north272[] = {272};
static const int16_t north273[] = {273};
static const int16_t north274[] = {274};
static const int16_t north275[] = {275};
static const int16_t north276[] = {276};
static const int16_t north277[] = {277};
static const int16_t north278[] = {278};
static const int16_t north279[] = {279};
static const int16_t north280[] = {280};
static const int16_t north281[] = {281};
static const int16_t north282[] = {282};
static const int16_t north283[] = {283};
static const int16_t north284[] = {284};
static const int16_t north285[] = {285};
static const int16_t north286[] = {286};
static const int16_t north287[] = {287};
static const int16_t north288[] = {288};
static const int16_t north289[] = {289};
static const int16_t north290[] = {290};
static const int16_t north291[] = {291};
static const int16_t north292[] = {292};
static const int16_t north293[] = {293};
static const int16_t north294[] = {294};
static const int16_t north295[] = {295};
static const int16_t north296[] = {296};
static const int16_t north297[] = {297};
static const int16_t north298[] = {298};
static const int16_t north299[] = {299};
static const int16_t north300[] = {300};
static const int16_t north301[] = {301};
static const int16_t north302[] = {302};
static const int16_t north303[] = {303};
static const int16_t north304[] = {304};
static const int16_t north305[] = {305};
static const int16_t north306[] = {306};
static const int16_t north307[] = {307};
static const int16_t north308[] = {308};
static const int16_t north309[] = {309};
static const int16_t north310[] = {310};
static const int16_t north311[] = {311};
static const int16_t north312[] = {312};
static const int16_t north313[] = {313};
static const int16_t north314[] = {314};
static const int16_t north315[] = {315};
static const int16_t north316[] = {316};
static const int16_t north317[] = {317};
static const int16_t north318[] = {318};
static const int16_t north319[] = {319};
static const int16_t north320[] = {320};
static const int16_t north321[] = {321};
static const int16_t north322[] = {-3, 322, 323, 324};
static const int16_t north329[] = {-2, 329, 330};

/* An array containing all southbound LED mappings */
static const LEDLoc southLEDLocs[] = {
    {south1, 34.456611,-119.976601,73}, // 1
    {south2, 34.441698,-119.949172,73}, // 2
    // {south4, 34.432329,-119.886923,72}, // 4
    // {south6, 34.437605,-119.852141,70}, // 6
    // {south7, 34.438845,-119.837473,68}, // 7
    // {south8, 34.441526,-119.812123,65}, // 8
    // {south9, 34.442127,-119.790592,68}, // 9
    // {south10, 34.44097,-119.764657,67}, // 10
    // {south11, 34.437042,-119.75204,67}, // 11
    // {south12, 34.428914,-119.739875,69}, // 12
    // {south13, 34.418469,-119.710828,69}, // 13
    // {south14, 34.417569,-119.690316,66}, // 14
    // {south15, 34.420173,-119.678777,66}, // 15
    // {south16, 34.42007,-119.646201,63}, // 16
    // {south17, 34.420893,-119.625925,59}, // 17
    // {south18, 34.420993,-119.603676,65}, // 18
    // {south19, 34.415409,-119.574954,68}, // 19
    // {south21, 34.404897,-119.535713,70}, // 21
    // {south23, 34.386948,-119.494104,68}, // 23
    // {south24, 34.376359,-119.478283,71}, // 24
    // {south25, 34.371952,-119.457089,71}, // 25
    // {south27, 34.341337,-119.412232,71}, // 27
    // {south34, 34.277325,-119.294717,68}, // 34
    // {south35, 34.271576,-119.276704,68}, // 35
    // {south36, 34.258928,-119.254667,67}, // 36
    // {south37, 34.26286,-119.232378,68}, // 37
    // {south38, 34.257915,-119.221974,70}, // 38
    // {south39, 34.244905,-119.196538,71}, // 39
    // {south40, 34.232881,-119.174335,70}, // 40
    // {south41, 34.223428,-119.151889,72}, // 41
    // {south42, 34.222045,-119.133645,72}, // 42
    // {south43, 34.221897,-119.112273,71}, // 43
    // {south44, 34.21978,-119.079481,70}, // 44
    // {south45, 34.218229,-119.060056,70}, // 45
    // {south46, 34.287723,-119.154833,34}, // 46
    // {south47, 34.275889,-119.138081,46}, // 47
    // {south48, 34.266846,-119.125049,48}, // 48
    // {south49, 34.261664,-119.10066,44}, // 49
    // {south50, 34.263607,-119.088638,55}, // 50
    // {south51, 34.263654,-119.056724,57}, // 51
    // {south52, 34.263619,-119.027337,56}, // 52
    // {south53, 34.263678,-119.009811,47}, // 53
    // {south54, 34.263438,-118.989869,51}, // 54
    // {south55, 34.209382,-119.142283,40}, // 55
    // {south56, 34.189284,-119.14253,51}, // 56
    // {south57, 34.16359,-119.141178,64}, // 57
    // {south58, 34.157032,-119.129014,66}, // 58
    // {south59, 34.150523,-119.116555,66}, // 59
    // {south60, 34.130404,-119.096348,67}, // 60
    // {south61, 34.111924,-119.081633,56}, // 61
    // {south65, 34.271867,-118.935143,50}, // 65
    // {south66, 34.27873,-118.920293,46}, // 66
    // {south67, 34.278827,-118.898894,40}, // 67
    // {south68, 34.27882,-118.870192,34}, // 68
    // {south69, 34.289848,-118.862482,68}, // 69
    // {south70, 34.293233,-118.841428,70}, // 70
    // {south73, 34.217153,-119.036096,70}, // 73
    // {south74, 34.215518,-119.014259,72}, // 74
    // {south75, 34.210715,-118.999043,71}, // 75
    // {south76, 34.200915,-118.96635,67}, // 76
    // {south78, 34.184936,-118.928123,71}, // 78
    // {south79, 34.184278,-118.904615,70}, // 79
    // {south80, 34.180732,-118.88622,68}, // 80
    // {south81, 34.176119,-118.861062,70}, // 81
    // {south85, 34.058914,-118.97422,57}, // 85
    // {south87, 34.046471,-118.930979,58}, // 87
    // {south89, 34.0406,-118.88858,55}, // 89
    // {south91, 34.282068,-118.780148,71}, // 91
    // {south92, 34.282041,-118.756808,70}, // 92
    // {south93, 34.28169,-118.719398,71}, // 93
    // {south94, 34.281381,-118.693,71}, // 94
    // {south95, 34.281362,-118.677214,73}, // 95
    // {south96, 34.275284,-118.657707,70}, // 96
    // {south98, 34.275228,-118.623954,71}, // 98
    // {south99, 34.274966,-118.597786,73}, // 99
    // {south100, 34.171983,-118.847118,71}, // 100
    // {south101, 34.157134,-118.824611,73}, // 101
    // {south102, 34.148115,-118.801464,73}, // 102
    // {south103, 34.147213,-118.7751,73}, // 103
    // {south104, 34.145757,-118.760073,73}, // 104
    // {south105, 34.141871,-118.73495,68}, // 105
    // {south106, 34.139261,-118.714151,71}, // 106
    // {south107, 34.150529,-118.695067,69}, // 107
    // {south111, 34.022107,-118.805529,40}, // 111
    // {south112, 34.025337,-118.781737,53}, // 112
    // {south114, 34.03324,-118.740492,51}, // 114
    // {south116, 34.034138,-118.686065,44}, // 116
    // {south118, 34.562778,-118.683605,72}, // 118
    // {south122, 34.491004,-118.620514,73}, // 122
    // {south123, 34.481393,-118.614747,71}, // 123
    // {south125, 34.44993,-118.613728,70}, // 125
    // {south126, 34.438529,-118.5972,68}, // 126
    // {south127, 34.416642,-118.580106,67}, // 127
    // {south128, 34.398218,-118.572524,66}, // 128
    // {south129, 34.381309,-118.567223,68}, // 129
    // {south130, 34.366025,-118.557295,68}, // 130
    // {south131, 34.349166,-118.540169,68}, // 131
    // {south133, 34.322656,-118.499685,71}, // 133
    // {south134, 34.313791,-118.48889,63}, // 134
    // {south135, 34.302183,-118.478828,71}, // 135
    // {south136, 34.273324,-118.574531,71}, // 136
    // {south137, 34.272172,-118.555466,70}, // 137
    // {south138, 34.277895,-118.527625,71}, // 138
    // {south140, 34.266358,-118.482471,70}, // 140
    // {south141, 34.265804,-118.4617,70}, // 141
    // {south142, 34.267551,-118.444533,73}, // 142
    // {south143, 34.276438,-118.423338,70}, // 143
    // {south144, 34.285404,-118.460485,71}, // 144
    // {south145, 34.159076,-118.638179,71}, // 145
    // {south146, 34.165278,-118.620755,70}, // 146
    // {south147, 34.170497,-118.604873,66}, // 147
    // {south148, 34.168024,-118.583226,71}, // 148
    // {south149, 34.173298,-118.562796,70}, // 149
    // {south150, 34.17318,-118.536657,69}, // 150
    // {south151, 34.171057,-118.51089,70}, // 151
    // {south152, 34.166253,-118.494541,68}, // 152
    // {south153, 34.159377,-118.46761,64}, // 153
    // {south155, 34.037228,-118.614858,47}, // 155
    // {south159, 34.032216,-118.528422,49}, // 159
    // {south160, 34.02092,-118.507822,40}, // 160
    // {south161, 34.011296,-118.495474,61}, // 161
    // {south162, 34.012907,-118.483852,21}, // 162
    // {south163, 34.323139,-118.472975,73}, // 163
    // {south164, 34.323122,-118.450021,74}, // 164
    // {south165, 34.305004,-118.426039,72}, // 165
    // {south166, 34.293749,-118.413918,73}, // 166
    // {south168, 34.27667,-118.388258,75}, // 168
    // {south169, 34.271367,-118.348393,69}, // 169
    // {south171, 34.245926,-118.321101,70}, // 171
    // {south172, 34.262909,-118.472341,73}, // 172
    // {south173, 34.254983,-118.472447,70}, // 173
    // {south174, 34.21043,-118.473462,70}, // 174
    // {south175, 34.189437,-118.47437,69}, // 175
    // {south176, 34.177868,-118.46896,67}, // 176
    // {south177, 34.144713,-118.471268,63}, // 177
    // {south178, 34.120631,-118.480359,65}, // 178
    // {south179, 34.106899,-118.479272,66}, // 179
    // {south180, 34.082384,-118.474319,69}, // 180
    // {south181, 34.070666,-118.465055,68}, // 181
    // {south182, 34.03722,-118.438988,66}, // 182
    // {south183, 34.037147,-118.438925,66}, // 183
    // {south184, 34.01729,-118.422727,68}, // 184
    // {south185, 34.003871,-118.412112,69}, // 185
    // {south186, 33.986709,-118.398472,66}, // 186
    // {south187, 33.971064,-118.37672,68}, // 187
    // {south188, 33.960197,-118.369467,70}, // 188
    // {south190, 33.996587,-118.458189,24}, // 190
    // {south191, 33.985153,-118.443377,27}, // 191
    // {south192, 33.97413,-118.431922,35}, // 192
    // {south194, 33.947764,-118.39644,34}, // 194
    // {south195, 33.930374,-118.396364,28}, // 195
    // {south196, 33.907197,-118.396209,36}, // 196
    // {south197, 33.88857,-118.396164,32}, // 197
    // {south198, 33.869464,-118.3945,32}, // 198
    // {south199, 33.850259,-118.388984,35}, // 199
    // {south200, 33.830459,-118.385181,35}, // 200
    // {south201, 33.816589,-118.380358,35}, // 201
    // {south202, 33.807033,-118.356468,27}, // 202
    // {south203, 33.797004,-118.339846,37}, // 203
    // {south204, 33.789443,-118.315331,25}, // 204
    // {south205, 33.79057,-118.295503,24}, // 205
    // {south206, 33.790425,-118.276143,26}, // 206
    // {south207, 33.791016,-118.245162,42}, // 207
    // {south208, 33.9167,-118.370317,71}, // 208
    // {south209, 33.897751,-118.370071,69}, // 209
    // {south210, 33.886981,-118.356054,70}, // 210
    // {south211, 33.869424,-118.33974,68}, // 211
    // {south212, 33.861153,-118.315464,68}, // 212
    // {south213, 33.859725,-118.298422,68}, // 213
    // {south214, 33.850394,-118.276562,71}, // 214
    // {south215, 33.832734,-118.257127,69}, // 215
    // {south216, 33.825768,-118.234892,70}, // 216
    // {south217, 33.825373,-118.215225,68}, // 217
    // {south218, 33.822181,-118.196994,70}, // 218
    // {south219, 33.813898,-118.169315,72}, // 219
    // {south220, 33.806498,-118.147991,70}, // 220
    // {south221, 33.802696,-118.128906,70}, // 221
    // {south222, 33.799287,-118.11126,71}, // 222
    // {south223, 33.787052,-118.094907,71}, // 223
    // {south224, 33.774193,-118.079238,73}, // 224
    // {south225, 33.774029,-118.051846,73}, // 225
    // {south227, 33.789959,-118.203148,37}, // 227
    // {south228, 33.789796,-118.174576,35}, // 228
    // {south229, 33.789782,-118.150086,30}, // 229
    // {south230, 33.785562,-118.136511,30}, // 230
    // {south231, 33.771047,-118.118424,37}, // 231
    // {south232, 33.75004,-118.106088,35}, // 232
    // {south233, 33.739377,-118.092321,45}, // 233
    // {south235, 34.252447,-118.430911,70}, // 235
    // {south236, 34.239215,-118.416954,71}, // 236
    // {south237, 34.231747,-118.40583,70}, // 237
    // {south238, 34.226629,-118.377998,68}, // 238
    // {south239, 34.221127,-118.359198,70}, // 239
    // {south240, 34.203281,-118.341182,73}, // 240
    // {south241, 34.181851,-118.314929,70}, // 241
    // {south242, 34.173495,-118.306654,69}, // 242
    // {south243, 34.158312,-118.2917,71}, // 243
    // {south244, 34.159101,-118.459768,69}, // 244
    // {south245, 34.155563,-118.432098,68}, // 245
    // {south246, 34.15652,-118.40416,66}, // 246
    // {south247, 34.153848,-118.378334,62}, // 247
    // {south248, 34.135782,-118.3604,66}, // 248
    // {south249, 34.126847,-118.345282,65}, // 249
    // {south250, 34.109629,-118.333707,65}, // 250
    // {south251, 34.104134,-118.321012,66}, // 251
    // {south252, 34.087644,-118.3045,59}, // 252
    // {south254, 34.233062,-118.275668,73}, // 254
    // {south255, 34.228394,-118.257812,75}, // 255
    // {south256, 34.219222,-118.241718,73}, // 256
    // {south257, 34.207711,-118.218271,73}, // 257
    // {south258, 34.20451,-118.195337,75}, // 258
    // {south259, 34.190524,-118.181257,73}, // 259
    // {south260, 34.177294,-118.164022,73}, // 260
    // {south261, 34.1575,-118.15881,65}, // 261
    // {south262, 34.147053,-118.279345,69}, // 262
    // {south263, 34.12857,-118.274084,69}, // 263
    // {south264, 34.109602,-118.261896,64}, // 264
    // {south265, 34.097012,-118.245523,64}, // 265
    // {south266, 34.085574,-118.231985,65}, // 266
    // {south267, 34.070438,-118.218139,62}, // 267
    // {south268, 34.055212,-118.189082,69}, // 268
    // {south269, 34.061226,-118.167009,68}, // 269
    // {south270, 34.070551,-118.146695,65}, // 270
    // {south271, 34.076987,-118.281161,48}, // 271
    // {south272, 34.069188,-118.260149,55}, // 272
    // {south273, 34.058469,-118.244867,59}, // 273
    // {south274, 34.045881,-118.221368,61}, // 274
    // {south275, 34.03018,-118.216996,64}, // 275
    // {south276, 34.023257,-118.200851,66}, // 276
    // {south277, 34.019733,-118.180838,63}, // 277
    // {south278, 34.012865,-118.164861,62}, // 278
    // {south279, 33.992133,-118.14214,65}, // 279
    // {south280, 34.151545,-118.139586,71}, // 280
    // {south281, 34.152106,-118.120279,70}, // 281
    // {south282, 34.152326,-118.096114,72}, // 282
    // {south283, 34.147663,-118.076986,69}, // 283
    // {south284, 34.148529,-118.058494,70}, // 284
    // {south285, 34.144422,-118.024516,70}, // 285
    // {south286, 34.13559,-118.007956,68}, // 286
    // {south287, 34.135297,-117.993053,69}, // 287
    // {south288, 34.135381,-117.966244,69}, // 288
    // {south289, 34.071413,-118.129822,68}, // 289
    // {south290, 34.071809,-118.097164,68}, // 290
    // {south291, 34.072031,-118.077432,69}, // 291
    // {south293, 34.0678,-118.032729,68}, // 293
    // {south294, 34.064205,-118.010711,68}, // 294
    // {south295, 34.066216,-117.990792,65}, // 295
    // {south296, 34.070264,-117.960005,62}, // 296
    // {south297, 34.072271,-117.938574,71}, // 297
    // {south298, 33.978561,-118.127752,64}, // 298
    // {south299, 33.962397,-118.118245,66}, // 299
    // {south300, 33.947338,-118.100895,66}, // 300
    // {south301, 33.931184,-118.090132,72}, // 301
    // {south302, 33.909002,-118.069536,72}, // 302
    // {south303, 33.896479,-118.051894,73}, // 303
    // {south304, 33.888342,-118.037136,73}, // 304
    // {south305, 33.878396,-118.018812,73}, // 305
    // {south306, 33.867764,-118.001069,71}, // 306
    // {south307, 33.830557,-117.930546,70}, // 307
    // {south308, 33.837759,-117.945675,77}, // 308
    // {south309, 33.849341,-117.967154,71}, // 309
    // {south310, 33.859422,-117.986492,71}, // 310
    // {south311, 34.071919,-117.899631,70}, // 311
    // {south312, 34.072067,-117.915335,70}, // 312
    // {south313, 34.120911,-117.901181,68}, // 313
    // {south314, 34.123108,-117.913199,66}, // 314
    // {south315, 34.129843,-117.944324,68}, // 315
    // {south316, 33.810332,-117.910352,69}, // 316
    // {south317, 33.792478,-117.891171,70}, // 317
    // {south318, 33.691012,-117.924469,73}, // 318
    // {south319, 33.702677,-117.951453,75}, // 319
    // {south320, 33.769072,-118.034357,75}, // 320
    // {south322, 33.738672,-117.997969,74}, // 322
    // {south324, 33.717456,-117.969274,74}, // 324
    // {south329, 33.707656,-118.059668,56}, // 329
};

/* An array containing all northbound LED mappings */
static const LEDLoc northLEDLocs[] = {
    {north1, 34.457053,-119.976493,72}, // 1
    {north2, 34.439879,-119.933952,72}, // 2
    {north4, 34.432943,-119.895871,72}, // 4
    {north6, 34.43537,-119.865932,71}, // 6
    {north7, 34.439114,-119.837874,68}, // 7
    // {north8, 34.441734,-119.812004,68}, // 8
    // {north9, 34.442433,-119.788286,70}, // 9
    // {north10, 34.441277,-119.760954,70}, // 10
    // {north11, 34.437371,-119.752066,67}, // 11
    // {north12, 34.427993,-119.733004,67}, // 12
    // {north13, 34.415854,-119.706396,65}, // 13
    // {north14, 34.419683,-119.686107,68}, // 14
    // {north15, 34.422436,-119.661545,63}, // 15
    // {north16, 34.420181,-119.645521,64}, // 16
    // {north17, 34.421038,-119.62556,62}, // 17
    // {north18, 34.421276,-119.603251,66}, // 18
    // {north19, 34.415857,-119.574269,66}, // 19
    // {north20, 34.407555,-119.548256,67}, // 20
    // {north22, 34.39568,-119.51064,72}, // 22
    // {north23, 34.386845,-119.492883,71}, // 23
    // {north24, 34.376751,-119.478378,68}, // 24
    // {north25, 34.372389,-119.457165,71}, // 25
    // {north33, 34.280345,-119.309791,71}, // 33
    // {north34, 34.277632,-119.294203,67}, // 34
    // {north36, 34.259338,-119.253634,71}, // 36
    // {north37, 34.262925,-119.231815,70}, // 37
    // {north39, 34.24512,-119.196236,70}, // 39
    // {north40, 34.232496,-119.173518,71}, // 40
    // {north41, 34.223585,-119.15094,70}, // 41
    // {north42, 34.222307,-119.133612,70}, // 42
    // {north43, 34.222203,-119.112555,71}, // 43
    // {north44, 34.220063,-119.079302,71}, // 44
    // {north45, 34.218511,-119.059988,71}, // 45
    // {north46, 34.287776,-119.15454,37}, // 46
    // {north47, 34.276064,-119.137882,49}, // 47
    // {north48, 34.266934,-119.124899,48}, // 48
    // {north49, 34.262247,-119.100252,42}, // 49
    // {north50, 34.263793,-119.088527,56}, // 50
    // {north51, 34.263802,-119.056712,57}, // 51
    // {north52, 34.263903,-119.027198,56}, // 52
    // {north53, 34.26385,-119.009773,52}, // 53
    // {north54, 34.263996,-118.988073,50}, // 54
    // {north55, 34.201247,-119.142155,39}, // 55
    // {north56, 34.190692,-119.14227,35}, // 56
    // {north57, 34.162824,-119.140047,63}, // 57
    // {north58, 34.157241,-119.128871,65}, // 58
    // {north59, 34.151236,-119.117333,65}, // 59
    // {north60, 34.130488,-119.096101,65}, // 60
    // {north61, 34.112049,-119.08137,55}, // 61
    // {north65, 34.272083,-118.935213,52}, // 65
    // {north66, 34.279066,-118.919682,52}, // 66
    // {north67, 34.279068,-118.898592,42}, // 67
    // {north68, 34.279451,-118.870274,36}, // 68
    // {north69, 34.291527,-118.863498,70}, // 69
    // {north70, 34.293671,-118.84141,71}, // 70
    // {north72, 34.283681,-118.79144,73}, // 72
    // {north73, 34.21728,-119.035788,70}, // 73
    // {north75, 34.211103,-118.999179,69}, // 75
    // {north76, 34.201044,-118.965411,69}, // 76
    // {north78, 34.185113,-118.927825,71}, // 78
    // {north79, 34.184546,-118.903716,71}, // 79
    // {north80, 34.180818,-118.885794,71}, // 80
    // {north81, 34.176314,-118.860531,73}, // 81
    // {north85, 34.059025,-118.973987,57}, // 85
    // {north87, 34.046911,-118.930416,60}, // 87
    // {north89, 34.040809,-118.888455,54}, // 89
    // {north91, 34.282505,-118.780055,73}, // 91
    // {north92, 34.282498,-118.756577,73}, // 92
    // {north93, 34.282066,-118.718667,72}, // 93
    // {north94, 34.281847,-118.692631,72}, // 94
    // {north95, 34.28179,-118.676923,73}, // 95
    // {north96, 34.27497,-118.656465,71}, // 96
    // {north98, 34.275767,-118.624011,71}, // 98
    // {north99, 34.275338,-118.597219,70}, // 99
    // {north101, 34.157069,-118.823973,71}, // 101
    // {north102, 34.148302,-118.801168,71}, // 102
    // {north103, 34.14751,-118.775254,71}, // 103
    // {north104, 34.145974,-118.759422,72}, // 104
    // {north105, 34.141988,-118.734519,68}, // 105
    // {north106, 34.141859,-118.707596,71}, // 106
    // {north107, 34.151224,-118.694015,70}, // 107
    // {north111, 34.022395,-118.805204,41}, // 111
    // {north112, 34.025598,-118.781624,53}, // 112
    // {north114, 34.033513,-118.74058,54}, // 114
    // {north116, 34.034463,-118.686594,45}, // 116
    // {north118, 34.563247,-118.682947,64}, // 118
    // {north122, 34.491188,-118.620022,72}, // 122
    // {north125, 34.450118,-118.613421,71}, // 125
    // {north126, 34.438822,-118.596958,71}, // 126
    // {north127, 34.416685,-118.579657,72}, // 127
    // {north128, 34.398391,-118.571989,73}, // 128
    // {north129, 34.381592,-118.566743,70}, // 129
    // {north130, 34.366212,-118.556828,70}, // 130
    // {north131, 34.349448,-118.539633,69}, // 131
    // {north133, 34.322942,-118.499216,68}, // 133
    // {north134, 34.313868,-118.488527,70}, // 134
    // {north135, 34.302336,-118.47851,69}, // 135
    // {north136, 34.273831,-118.574406,71}, // 136
    // {north137, 34.272463,-118.555402,71}, // 137
    // {north138, 34.278298,-118.527574,71}, // 138
    // {north139, 34.275761,-118.494204,70}, // 139
    // {north140, 34.266818,-118.481961,70}, // 140
    // {north141, 34.266035,-118.461867,71}, // 141
    // {north142, 34.267579,-118.444133,72}, // 142
    // {north143, 34.276815,-118.423805,72}, // 143
    // {north144, 34.285578,-118.460261,69}, // 144
    // {north145, 34.159239,-118.638295,70}, // 145
    // {north146, 34.165465,-118.620697,70}, // 146
    // {north150, 34.173412,-118.536613,68}, // 150
    // {north151, 34.171323,-118.510931,68}, // 151
    // {north152, 34.166415,-118.494344,68}, // 152
    // {north153, 34.160171,-118.468003,65}, // 153
    // {north155, 34.037435,-118.614797,49}, // 155
    // {north158, 34.038795,-118.556696,45}, // 158
    // {north159, 34.03246,-118.527952,46}, // 159
    // {north160, 34.021134,-118.507589,42}, // 160
    // {north161, 34.011494,-118.495462,52}, // 161
    // {north162, 34.013062,-118.483706,23}, // 162
    // {north163, 34.323484,-118.472919,71}, // 163
    // {north164, 34.323412,-118.449697,72}, // 164
    // {north165, 34.305259,-118.425635,70}, // 165
    // {north166, 34.294024,-118.413489,70}, // 166
    // {north167, 34.279552,-118.397988,73}, // 167
    // {north168, 34.274268,-118.372438,75}, // 168
    // {north169, 34.271787,-118.348083,76}, // 169
    // {north170, 34.255423,-118.325374,75}, // 170
    // {north172, 34.262918,-118.472088,69}, // 172
    // {north173, 34.255058,-118.472138,91}, // 173
    // {north174, 34.210439,-118.473171,68}, // 174
    // {north175, 34.189443,-118.474101,68}, // 175
    // {north176, 34.177995,-118.468665,66}, // 176
    // {north177, 34.144659,-118.470907,67}, // 177
    // {north178, 34.120331,-118.48023,66}, // 178
    // {north179, 34.106971,-118.47892,64}, // 179
    // {north180, 34.082475,-118.473989,66}, // 180
    // {north181, 34.070743,-118.464738,66}, // 181
    // {north182, 34.037344,-118.438692,61}, // 182
    // {north183, 34.037345,-118.438667,60}, // 183
    // {north184, 34.017431,-118.422386,73}, // 184
    // {north185, 34.003984,-118.411766,67}, // 185
    // {north187, 33.971233,-118.3766,68}, // 187
    // {north188, 33.960215,-118.369204,68}, // 188
    // {north189, 33.932232,-118.368282,70}, // 189
    // {north190, 33.996763,-118.45813,32}, // 190
    // {north191, 33.985325,-118.443005,21}, // 191
    // {north192, 33.971534,-118.429679,34}, // 192
    // {north193, 33.95471,-118.413525,43}, // 193
    // {north194, 33.946563,-118.396006,25}, // 194
    // {north195, 33.930176,-118.395957,24}, // 195
    // {north196, 33.907137,-118.395667,38}, // 196
    // {north197, 33.889791,-118.395844,38}, // 197
    // {north198, 33.869658,-118.394298,30}, // 198
    // {north199, 33.850348,-118.388818,27}, // 199
    // {north200, 33.830434,-118.385024,32}, // 200
    // {north201, 33.816741,-118.380191,29}, // 201
    // {north202, 33.807137,-118.356161,34}, // 202
    // {north203, 33.797118,-118.339633,39}, // 203
    // {north204, 33.789626,-118.315793,29}, // 204
    // {north205, 33.790839,-118.295303,30}, // 205
    // {north206, 33.790557,-118.276001,29}, // 206
    // {north207, 33.791557,-118.254168,39}, // 207
    // {north209, 33.897773,-118.369716,66}, // 209
    // {north210, 33.886986,-118.355686,67}, // 210
    // {north211, 33.869564,-118.339428,68}, // 211
    // {north212, 33.861382,-118.315434,67}, // 212
    // {north213, 33.859961,-118.298424,65}, // 213
    // {north214, 33.85056,-118.276311,68}, // 214
    // {north215, 33.83282,-118.256873,68}, // 215
    // {north216, 33.825978,-118.234522,71}, // 216
    // {north217, 33.82563,-118.215041,71}, // 217
    // {north218, 33.822382,-118.196792,71}, // 218
    // {north219, 33.81412,-118.169288,70}, // 219
    // {north221, 33.802896,-118.128995,71}, // 221
    // {north222, 33.799472,-118.11113,70}, // 222
    // {north223, 33.787196,-118.094706,70}, // 223
    // {north224, 33.774665,-118.078965,69}, // 224
    // {north225, 33.774574,-118.051955,73}, // 225
    // {north227, 33.790123,-118.202978,38}, // 227
    // {north228, 33.789964,-118.174527,25}, // 228
    // {north229, 33.789944,-118.149952,22}, // 229
    // {north230, 33.78563,-118.136205,30}, // 230
    // {north231, 33.770698,-118.117986,37}, // 231
    // {north232, 33.750057,-118.105799,35}, // 232
    // {north233, 33.732096,-118.084671,43}, // 233
    // {north235, 34.248692,-118.426293,69}, // 235
    // {north237, 34.231436,-118.402399,70}, // 237
    // {north238, 34.226915,-118.377908,68}, // 238
    // {north240, 34.203507,-118.340761,70}, // 240
    // {north241, 34.181786,-118.314375,71}, // 241
    // {north242, 34.173378,-118.306214,65}, // 242
    // {north243, 34.158107,-118.291191,66}, // 243
    // {north244, 34.159343,-118.459551,67}, // 244
    // {north245, 34.155804,-118.427593,68}, // 245
    // {north246, 34.156723,-118.40357,66}, // 246
    // {north247, 34.154602,-118.378667,63}, // 247
    // {north248, 34.135103,-118.358542,66}, // 248
    // {north249, 34.126867,-118.344947,61}, // 249
    // {north250, 34.111701,-118.335281,62}, // 250
    // {north255, 34.229006,-118.25778,72}, // 255
    // {north256, 34.219668,-118.241382,73}, // 256
    // {north257, 34.208029,-118.218138,71}, // 257
    // {north259, 34.190577,-118.180848,70}, // 259
    // {north260, 34.17776,-118.163803,74}, // 260
    // {north261, 34.157613,-118.158147,69}, // 261
    // {north262, 34.147038,-118.278925,66}, // 262
    // {north263, 34.12828,-118.273579,68}, // 263
    // {north264, 34.109885,-118.261704,66}, // 264
    // {north265, 34.097099,-118.24516,65}, // 265
    // {north266, 34.085671,-118.231697,68}, // 266
    // {north267, 34.070492,-118.217891,65}, // 267
    // {north268, 34.055485,-118.189044,71}, // 268
    // {north269, 34.061452,-118.166883,66}, // 269
    // {north270, 34.071012,-118.146617,64}, // 270
    // {north271, 34.077016,-118.280464,63}, // 271
    // {north272, 34.069678,-118.260805,60}, // 272
    // {north273, 34.058403,-118.244457,51}, // 273
    // {north274, 34.044369,-118.220952,60}, // 274
    // {north275, 34.03011,-118.216477,63}, // 275
    // {north276, 34.023701,-118.201273,57}, // 276
    // {north277, 34.019931,-118.180786,60}, // 277
    // {north278, 34.012955,-118.164488,63}, // 278
    // {north279, 33.992285,-118.141885,65}, // 279
    // {north280, 34.152105,-118.139493,67}, // 280
    // {north281, 34.152567,-118.120292,69}, // 281
    // {north282, 34.152816,-118.095489,66}, // 282
    // {north283, 34.148215,-118.076951,67}, // 283
    // {north284, 34.149031,-118.058771,67}, // 284
    // {north285, 34.144762,-118.024184,66}, // 285
    // {north286, 34.135899,-118.00773,68}, // 286
    // {north287, 34.135621,-117.992902,71}, // 287
    // {north288, 34.135743,-117.966309,70}, // 288
    // {north289, 34.0717,-118.129846,66}, // 289
    // {north290, 34.072152,-118.097058,67}, // 290
    // {north291, 34.07236,-118.077475,66}, // 291
    // {north292, 34.072536,-118.066822,68}, // 292
    // {north293, 34.068096,-118.032812,70}, // 293
    // {north294, 34.064452,-118.01046,68}, // 294
    // {north295, 34.066504,-117.990825,66}, // 295
    // {north296, 34.070528,-117.959994,68}, // 296
    // {north297, 34.072573,-117.938512,70}, // 297
    // {north298, 33.978687,-118.127536,64}, // 298
    // {north299, 33.96221,-118.11753,63}, // 299
    // {north300, 33.947458,-118.100593,62}, // 300
    // {north301, 33.928637,-118.087589,68}, // 301
    // {north302, 33.909114,-118.068979,71}, // 302
    // {north303, 33.896982,-118.052099,71}, // 303
    // {north304, 33.888552,-118.036757,72}, // 304
    // {north305, 33.878508,-118.018369,73}, // 305
    // {north306, 33.868041,-118.000982,73}, // 306
    // {north307, 33.830885,-117.93022,69}, // 307
    // {north308, 33.835122,-117.939021,69}, // 308
    // {north309, 33.849329,-117.966168,71}, // 309
    // {north310, 33.858657,-117.983143,70}, // 310
    // {north311, 34.072227,-117.899615,70}, // 311
    // {north312, 34.072379,-117.915308,71}, // 312
    // {north313, 34.12129,-117.901168,66}, // 313
    // {north314, 34.123158,-117.912503,66}, // 314
    // {north315, 34.130274,-117.944079,70}, // 315
    // {north316, 33.810584,-117.910049,70}, // 316
    // {north317, 33.792586,-117.890727,69}, // 317
    // {north318, 33.691284,-117.923817,73}, // 318
    // {north319, 33.702315,-117.95005,73}, // 319
    // {north320, 33.769271,-118.033955,74}, // 320
    // {north321, 33.753896,-118.01552,74}, // 321
    // {north322, 33.735142,-117.992554,74}, // 322
    // {north329, 33.707936,-118.059495,56}, // 329
};

#endif /* LED_LOCATIONS_ARRS_H_ */