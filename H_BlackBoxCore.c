/* 
　　初めに
バージョン情報：0.0.0

コード著者：七星破軍
実装ゲーム：桜降る代に決闘を　新幕
ゲーム制作者：bakafire,TOKIAME
このコードの私に掛かる著作権はフリーとします。必要があれば各自で改変・利用・再配布などお好みでどうぞ。
ただし、ゲームメカニクスについての知的財産権は先述のゲーム制作者に存在します。ご注意下さい。

個人的thanks：なまさん。（似たようなものを製作されています。また実装について相談させて頂きました）
個人的thanks：沢渡サキさん。（似たようなものを製作されています。参考と、触発になりました）
個人的thanks：Yoshiさん。（プログラミングに関することでいくつかお世話になりました）
specialthanks：「ゆるよにサーバー」の皆さん。特に質問板の住人の方。（実装上の疑問を解決する助けになりました）
ありがとうございました。
*/

/*
　　コードに関して
まず、著者はプログラミング初心者です。クロスコンパイルとかライブラリのリンクとか難しいことは言わないで下さい。
プログラムファイルなんて１枚しか作れません。ここに全部書きます。.datに分けません。privateも知りません。
あと初心者なのでgoto文が好きです。大目に見て下さい。
一応includeにOS固有のものは使っていないのでお手元のコンパイラをかませて頂くことは（多分）可能なはずです。
コードエディタで関数定義は畳んでしまうと見やすいと思います。あくまで本体はmain()で、関数は処理をまとめているだけなので。


    実装に関して
この実装では、「オペレーション」を軸に話が進みます。１つのオペレーションは、盤面変化のブロック単位だと思って下さい。
例えば開始フェイズ規定処理は、「集中+1」「付与落ち」「再構成」「ドロー」×2の４種類に分割でき、
このうち「再構成」以外は、これ以上分割できない最小単位です。要するに、「集中+1」の間に何かの処理が挟まることはありません。
再構成は、更に「ライフダメージ」と「リシャッフル」に分割できます。
このように、ふるよににおいて盤面が変化しうる、即ち何らかの状況起因解決が発生しうるまでの行動を１ブロックにしたものがオペレーションです

ふるよには入れ子解決、即ち処理のラストインファストアウトを行うため、このオペレーションをスタックに積んで上から解決していきます。

そして１つのオペレーションの終了ごとに、状況起因の判定を行います。この時それが起きていれば、その解決をスタックの上に積みます。
例えば「付与落ち」のオペレーションをしたサイクルの状況起因判定で、結晶が０になった付与があれば、
その「破棄時効果」の解決をスタックに積み、次のサイクルでは先客の「再構成」よりも先にそちらから解決されます。

この考え方を基本として実装を行っています。

また、変数名はなるべく英語版（level99games版）のSAKURA-ARMSの用語に基づいて設定しています。
視点となるプレイヤーをSubjectかController（長いのでCON）と表現していることが多いです。
プログラム中多く出てくるPA[CON].という表記は、オーラや山札などプレイヤーごとに存在する領域を、
「だれのどの領域」という表現で書くときに使っています。
*/

/*
    アップデート・変更点
現在のバージョン：0.0.1
以前のバージョン：0.0.0

・オペレーションが持つ詳細情報の内、カードに関連した部分を修正
　＞かなり強引な仕様ですが、対象にとるカードが存在する領域のカードの枚数も参照で受け取るように変更
　＞領域を移動させた際、移動元領域のカード枚数を１枚減ずる必要がありますが、領域ごとに構造体に互換性がなく、
　　こちらを改修するのは莫大な変更箇所を生み、手間と判断したため。

・オペコード4を追加
・オペコード11の仮組み
・オペコード12に追記
・オペコード19の完成
・オペコード20を追加
・オペコード21を追加、完成

・いくつかの関数の追加
*/





#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>

// 以下50行 メルセンヌツイスタで利用
#define MT_N 624
#define MT_M 397
#define MATRIX_A 0x9908b0dfUL
#define UPPER_MASK 0x80000000UL
#define LOWER_MASK 0x7fffffffUL
static unsigned long mt[MT_N]; // 乱数を格納する配列
static int mti=MT_N+1; // mti==MT_N+1はmt[]が初期化されていないことを意味する

// 乱数生成器を初期化する関数
void init_genrand(unsigned long s) {
    mt[0]= s & 0xffffffffUL;
    for (mti=1; mti<MT_N; mti++) {
        mt[mti] = (1812433253UL * (mt[mti-1] ^ (mt[mti-1] >> 30)) + mti);
        mt[mti] &= 0xffffffffUL; // 32ビットに収める
    }
}

// 32ビットの乱数を生成する関数
unsigned long genrand_int32(void) {
    unsigned long y;
    static unsigned long mag01[2]={0x0UL, MATRIX_A}; // これは、MATRIX_Aの値を使うためのテーブル
    if (mti >= MT_N) { // mt[]が全て使用されたら、新しい配列を生成
        int kk;
        if (mti == MT_N+1)   // 初期化されていない場合、デフォルトのシードで初期化
            init_genrand(5489UL);

        for (kk=0;kk<MT_N-MT_M;kk++) {
            y = (mt[kk]&UPPER_MASK)|(mt[kk+1]&LOWER_MASK);
            mt[kk] = mt[kk+MT_M] ^ (y >> 1) ^ mag01[y & 0x1UL];
        }
        for (;kk<MT_N-1;kk++) {
            y = (mt[kk]&UPPER_MASK)|(mt[kk+1]&LOWER_MASK);
            mt[kk] = mt[kk+(MT_M-MT_N)] ^ (y >> 1) ^ mag01[y & 0x1UL];
        }
        y = (mt[MT_N-1]&UPPER_MASK)|(mt[0]&LOWER_MASK);
        mt[MT_N-1] = mt[MT_M-1] ^ (y >> 1) ^ mag01[y & 0x1UL];
        mti = 0;
    }

    y = mt[mti++];

    // テンパリング処理
    y ^= (y >> 11);
    y ^= (y << 7) & 0x9d2c5680UL;
    y ^= (y << 15) & 0xefc60000UL;
    y ^= (y >> 18);

    return y;
}
// メルセンヌツイスタ関係 ここまで



// 総合ルール6-2に基づいてオブジェクト・カードを定義する（登場順）
typedef struct Object_Card {
    int classification ;    // 分類。0:通常札，1:切り札　のいずれか。常時公開情報
    char name[40] ;     // カードの名前。
    int cardID ;    // カード番号。NAとメガミ名を省略して、オリジンから登場順に数字化。通常札,切札,追加札は01,02,03と数字化。斬なら1-01-01-001で10101001
    int goddess_data ;      // メガミ情報。通常はメガミナンバーそのまま。メガミ情報なしは0を代入。
    int type_main ;     // カードタイプ。1:攻撃　2:行動　3:付与　4:不定　のいずれか
    int type_sub ;      // サブタイプ。 0:持たない　1:対応　2:全力　3:不定　のいずれか
    // カードテキストは持たせておく意味ある？　取り敢えず無視する。
    bool range_list[30] ;    // 適正距離。-10 ~ 19 のそれぞれの距離について持つ持たないのT/F
    int damage_aura ;   // 値-1を「-」として扱う事にする。攻撃札でない場合は-999。変数で表現されているなら-100
    int damage_life ;   // 値-1を「-」として扱う事にする。攻撃札でない場合は-999。変数で表現されているなら-100
    int charge ;    // 納。攻撃や行動札の場合は-999でも入れとこうかな。
    int cost ;      // 消費。切札でない場合は-999でも入れておく。

    bool face_up ;  // 表向きかどうか。
    int play_status ;   // 0:未使用　1:使用中　2:使用済み　のいずれか。切札でなければ-999

    int attached_SAKURA ;   // 紐付けされた桜花結晶の数
    int owner ; // 所有者情報。0:先手プレイヤーＡ　1:後手プレイヤーＢ　のいずれか
    int identity ;  // デュープリギアを初めとして盤上で完全に同ステータスになりうるカードの個人を識別
} Object_Card ;
// 0で全て初期化されたcard_formatdataを定義
Object_Card object_card_formatdata = {0} ;

// オブジェクト・アタックも同様に。ルール6-4による。
typedef struct Object_Attack {
    int classification ;    // 分類。-1:持たない　0:通常札　1:切り札　のいずれか。
    int type_sub ;      // サブタイプ。 0:持たない　1:対応　2:全力　のいずれか
    bool range_list[30] ;    // 適正距離。-10 ~ 19 のそれぞれの距離について持つ持たないのT/F
    int damage_aura ;   // 値-1を「-」として扱う事にする。変数で表現されているなら-100
    int damage_life ;   // 値-1を「-」として扱う事にする。変数で表現されているなら-100
    int goddess_data ;      // メガミ情報。通常はメガミナンバーそのまま。メガミ情報なしは0を代入。
    int text_data ;     // テキスト。
    bool unresponsive_all ;         // 対応不可（無条件）
    bool unresponsive_normal ;      // 対応不可（通常札）
    bool unresponsive_ultimate ;    // 対応不可（切札）
    bool unavoidable ;      // 不可避を持つかどうか
} Object_Attack ;


// 以下、総合ルール７章における各領域の定義を、構造体で行っていく。（同じ順）
typedef struct {
    int placed_SAKURA ;
} Area_Distance ;

typedef struct Area_Life_Personal {
    int placed_SAKURA ;
} Area_Life_Personal ;

typedef struct Area_Aura_Personal {
    int placed_SAKURA ;
    int placed_FREEZE ;
    int limit_upper ;
} Area_Aura_Personal ;

typedef struct Area_Flare_Personal {
    int placed_SAKURA ;
} Area_Flare_Personal ;

typedef struct {
    int placed_SAKURA ;
} Area_Shadow ;

typedef struct Area_Deck_Personal { //山札のスタックは順番情報が有意な事に注意
    int number_cards ;
    Object_Card cards_stack[20] ;
} Area_Deck_Personal ;

typedef struct Area_Played_Personal {
    int number_cards ;
    Object_Card cards_stack[20] ;
} Area_Played_Personal ;

typedef struct Area_Discard_Personal {
    int number_cards ;
    Object_Card cards_stack[20] ;
} Area_Discard_Personal ;

typedef struct Area_Enchant_Personal {
    int number_cards ;
    Object_Card cards_stack[20] ;
} Area_Enchant_Personal ;

typedef struct Area_Hand_Personal {
    int number_cards ;
    Object_Card cards_stack[20] ;
} Area_Hand_Personal ;

typedef struct Area_Ultimate_Personal {
    int number_cards ;
    Object_Card cards_stack[20] ;
} Area_Ultimate_Personal ;

typedef struct Area_Bonuscard_Personal {
    int number_cards ;
    Object_Card cards_stack[20] ;
} Area_Bonuscard_Personal ;

typedef struct Area_Suspend {
    int number_cards ;
    Object_Card cards_stack[20] ;
} Area_Suspend ;

typedef struct Area_Playing_Personal {
    int number_cards ;
    Object_Card cards_stack[20] ;
} Area_Playing_Personal ;

typedef struct Area_Attacking_Personal {
    int number_attacks ;
    Object_Attack attacks_stack[20] ;
} Area_Attacking_Personal ;

typedef struct {
    int placed_SAKURA ;
    int number_cards ;
    Object_Card cards_stack[20] ;
} Area_Outboard ;
// 以上、総合ルール７章における領域の定義


// これらの領域を構造体「Areas」として括る
struct Areas {
    Area_Distance area_distance ;
    struct Area_Life_Personal area_life_A ;
    struct Area_Life_Personal area_life_B ;
    struct Area_Aura_Personal area_aura_A ;
    struct Area_Aura_Personal area_aura_B ;
    struct Area_Flare_Personal area_flare_A ;
    struct Area_Flare_Personal area_flare_B ;
    Area_Shadow area_shadow ;
    struct Area_Deck_Personal area_deck_A ;
    struct Area_Deck_Personal area_deck_B ;
    struct Area_Played_Personal area_played_A ;
    struct Area_Played_Personal area_played_B ;
    struct Area_Discard_Personal area_discard_A ;
    struct Area_Discard_Personal area_discard_B ;
    struct Area_Enchant_Personal area_enchant_A ;
    struct Area_Enchant_Personal area_enchant_B ;
    struct Area_Hand_Personal area_hand_A ;
    struct Area_Hand_Personal area_hand_B ;
    struct Area_Ultimate_Personal area_ultimate_A ;
    struct Area_Ultimate_Personal area_ultimate_B ;
    struct Area_Bonuscard_Personal area_bonuscard_A ;
    struct Area_Bonuscard_Personal area_bonuscard_B ;
    Area_Suspend area_suspend ;
    struct Area_Playing_Personal area_playing_A ;
    struct Area_Playing_Personal area_playing_B ;
    struct Area_Attacking_Personal area_attacking_A ;
    struct Area_Attacking_Personal area_attacking_B ;
    Area_Outboard area_outboard ;
};
//そのままグローバルで宣言し、0で初期化
struct Areas areas = { 0 };


// コード内では「プレイヤーAのオーラ」等と記述する方が書きやすいので、それ用（別表記用）の構造体
typedef struct Player_Areas {
    Area_Life_Personal*     area_life ;
    Area_Aura_Personal*     area_aura ;
    Area_Flare_Personal*    area_flare ;
    Area_Deck_Personal*     area_deck ;
    Area_Played_Personal*   area_played ;
    Area_Discard_Personal*  area_discard ;
    Area_Enchant_Personal*  area_enchant ;
    Area_Hand_Personal*     area_hand ;
    Area_Ultimate_Personal* area_ultimate ;
    Area_Bonuscard_Personal*area_bonuscard ;
    Area_Playing_Personal*  area_playing ;
    Area_Attacking_Personal*area_attacking ;
} Player_Areas ;
// グローバルで初期化
Player_Areas PA[2] = { {&areas.area_life_A, &areas.area_aura_A, &areas.area_flare_A, &areas.area_deck_A, &areas.area_played_A, &areas.area_discard_A, &areas.area_enchant_A, &areas.area_hand_A, &areas.area_ultimate_A, &areas.area_bonuscard_A, &areas.area_playing_A, &areas.area_attacking_A} ,
                       {&areas.area_life_B, &areas.area_aura_B, &areas.area_flare_B, &areas.area_deck_B, &areas.area_played_B, &areas.area_discard_B, &areas.area_enchant_B, &areas.area_hand_B, &areas.area_ultimate_B, &areas.area_bonuscard_B, &areas.area_playing_B, &areas.area_attacking_B} };
// 別表記用の構造体関係、ここまで

// ルールにはないが、プログラム上、一部の効果をトークンとしてプレイヤーに付与する。
// トークンは、一度使用されると破棄されるもの。また各ターンの終了時に全て破棄される。（ターンを跨いで保持されない）
typedef struct Effect_Token {
    int kihaku ;    // 気迫のエンチャント効果。「次に行う」「他のメガミの」「切札でない」攻撃に、「距離拡大近１」と「対応不可（通常札）」を与える
    int seireirenkei ; // 精霊連携のエンチャント効果。
} Effect_Token ;

// ルール5-1　プレイヤーが持つステータス
typedef struct Player_Status {
    int  vigor ;
    int  vigor_limit_upper ;
    int  hand_limit_upper ;
    bool stun ;
    bool doing_reaction ;
    Effect_Token tokens ;   // 効果トークン（プログラム上の概念）を管理
} Player_Status ;

// ルール5-2　その他の（領域が持つものでない）情報をここにまとめる。プレイヤーステータスを包含する
typedef struct Other_Status {
    // 二人分の、プレイヤーステータスのデータ
    Player_Status player_status[2] ;

    // 以下、ゲーム進行上のデータ
    int turn_player ;
    int distance_turnstart ;
    
    int turn_number ;       // 現在のターン数
    int master_distance ;   // 達人の間合い
    
} Other_Status ;
// グローバルで宣言して初期化
Other_Status other_status = {0} ;





// 後述のstruct operationに含まれる細々とした情報色々
typedef struct Operation_Detail
{
    // ルールではなくカード効果によって発生した場合、代わりにそのカードを親としてIDを記憶
    int parent_cardID ;

    // 以下は「カード◯◯を使用する」のような限定状況でのみ利用する。対象カードのIDと存在領域と位置を保持。
    int process_cardID ;
    Object_Card* cards_stack;
    int* sauce_cards_num ;
    int card_position ;

} Operation_Detail ;

// メイン処理部分の要。この１つの断片、行動の最小単位をオペレーションと呼ぶことにする。スタックに積んでいく
typedef struct Operation
{
    int controller ;
    int ope_code ;
    int uniqueID ;
    int parent_uniqueID ;

    Operation_Detail detail ;   // その他の情報はここにつっこんでおく

} Operation ;
// 0で全て初期化されたoperation_formatdataを定義
Operation operation_formatdata = {0} ;

// オペレーションを積んで、上から順に処理していくスタックを定義（メイン処理ではここに積む→消化するの繰り返しで処理を表現する）
Operation ope_stack[5000] ;
// 合わせてスタックのカウンターも
int ope_number = 0 ;
int ope_IDused = 1 ;     // 固有IDのためのカウンター







// これのした
// 以下、関数の宣言


void DeckOfBeginningDuel ( Object_Card A_normal[], Object_Card A_ultimate[], Object_Card B_normal[], Object_Card B_ultimate[] ); 
void PreparForDuel ( Object_Card A_normal[], Object_Card A_ultimate[], Object_Card B_normal[], Object_Card B_ultimate[] );
int CalcNowDistance () ;
bool IsThereCard ( Object_Card area[], int cardID, int cardnum );
void ShuffleDeck ( int subject );
void DrawCard ( int subject );
Object_Attack RangeExpansionNear ( Object_Attack atk, int n );

void InteractionBoardOutput (int subject);
void OpponentUltimateOutput (int subject);
void OpponentPlayedOutput (int subject);
void OpponentEnchantOutput (int subject);
void SubjectEnchantOutput (int subject);
void SubjectHandOutput (int subject);
void SubjectUltimateOutput (int subject);
void SubjectPlayedOutput (int subject);
void SubjectDiscardOutput (int subject);

int UseCardOrBasicAction (int CON, bool fullpower, bool basicaction);
int AskBasicAction (int CON, bool must);
int AskPayBACost (int CON);

Object_Attack MakeGhostAttackObject ( Object_Card atkcard, int CON );
bool AvailableCards ( Object_Card card , int CON, bool fullpower);
bool CanPayBACost (int CON);
bool CanAdvance (int CON);
bool CanRetreat (int CON);
bool CanRecover (int CON);
bool CanFocus (int CON);
bool CanBreakaway (int CON);
int ReBAOpeCode ( int actionnum );


void push (Operation newope) ;
void StackRefresh ();
void RangeSet ( int min, int max, bool rangelist[] );
void SortCardStack ( Object_Card card_stack[], int position, int* cardnum );






// 以下、関数の定義


// 始まりの決闘に固有のデッキリストをプレイヤーA&Bの手札&切札として（自動的に）眼前構築する
void DeckOfBeginningDuel ( Object_Card A_normal[], Object_Card A_ultimate[], Object_Card B_normal[], Object_Card B_ultimate[] )
{
    // ウツロ・通常札群
    int i = 0 ;
    if (1) {
        Object_Card temp_card = { 0 } ;
        temp_card.classification = 0 ;
        memcpy(temp_card.name, "投射", sizeof(temp_card.name)) ;
        temp_card.cardID = 990101001 ;
        temp_card.goddess_data = 99 ;
        temp_card.type_main = 1 ;
        temp_card.type_sub = 0 ;
        RangeSet (5, 9, temp_card.range_list);
        temp_card.damage_aura = 3 ;
        temp_card.damage_life = 1 ;
        temp_card.charge = -999 ;
        temp_card.cost = -999 ;
        temp_card.owner = 0 ;
        A_normal[i++] = temp_card ; // 最後に、リストのi番目に代入
    }
    if (1) {
        Object_Card temp_card = { 0 } ;
        temp_card.classification = 0 ;
        memcpy(temp_card.name, "脇斬り", sizeof(temp_card.name)) ;
        temp_card.cardID = 990101002 ;
        temp_card.goddess_data = 99 ;
        temp_card.type_main = 1 ;
        temp_card.type_sub = 0 ;
        RangeSet (2, 3, temp_card.range_list);
        temp_card.damage_aura = 2 ;
        temp_card.damage_life = 2 ;
        temp_card.charge = -999 ;
        temp_card.cost = -999 ;
        temp_card.owner = 0 ;
        A_normal[i++] = temp_card ;
    }
    if (1) {
        Object_Card temp_card = { 0 } ;
        temp_card.classification = 0 ;
        memcpy(temp_card.name, "牽制", sizeof(temp_card.name)) ;
        temp_card.cardID = 990101003 ;
        temp_card.goddess_data = 99 ;
        temp_card.type_main = 1 ;
        temp_card.type_sub = 0 ;
        RangeSet (1, 3, temp_card.range_list);
        temp_card.damage_aura = 2 ;
        temp_card.damage_life = 1 ;
        temp_card.charge = -999 ;
        temp_card.cost = -999 ;
        temp_card.owner = 0 ;
        A_normal[i++] = temp_card ;
    }
    if (1) {
        Object_Card temp_card = { 0 } ;
        temp_card.classification = 0 ;
        memcpy(temp_card.name, "背中刺し", sizeof(temp_card.name)) ;
        temp_card.cardID = 990101004 ;
        temp_card.goddess_data = 99 ;
        temp_card.type_main = 1 ;
        temp_card.type_sub = 0 ;
        RangeSet (1, 1, temp_card.range_list);
        temp_card.damage_aura = 3 ;
        temp_card.damage_life = 2 ;
        temp_card.charge = -999 ;
        temp_card.cost = -999 ;
        temp_card.owner = 0 ;
        A_normal[i++] = temp_card ;
    }
    if (1) {
        Object_Card temp_card = { 0 } ;
        temp_card.classification = 0 ;
        memcpy(temp_card.name, "二刀一閃", sizeof(temp_card.name)) ;
        temp_card.cardID = 990101005 ;
        temp_card.goddess_data = 99 ;
        temp_card.type_main = 1 ;
        temp_card.type_sub = 2 ;
        RangeSet (2, 3, temp_card.range_list);
        temp_card.damage_aura = 4 ;
        temp_card.damage_life = 2 ;
        temp_card.charge = -999 ;
        temp_card.cost = -999 ;
        temp_card.owner = 0 ;
        A_normal[i++] = temp_card ;
        }
    if (1) {
        Object_Card temp_card = { 0 } ;
        temp_card.classification = 0 ;
        memcpy(temp_card.name, "歩法", sizeof(temp_card.name)) ;
        temp_card.cardID = 990101006 ;
        temp_card.goddess_data = 99 ;
        temp_card.type_main = 2 ;
        temp_card.type_sub = 0 ;
        RangeSet (-10, 19, temp_card.range_list) ;
        temp_card.damage_aura = -999 ;
        temp_card.damage_life = -999 ;
        temp_card.charge = -999 ;
        temp_card.cost = -999 ;
        temp_card.owner = 0 ;
        A_normal[i++] = temp_card ;
    }
    if (1) {
        Object_Card temp_card = { 0 } ;
        temp_card.classification = 0 ;
        memcpy(temp_card.name, "潜り", sizeof(temp_card.name)) ;
        temp_card.cardID = 990101007 ;
        temp_card.goddess_data = 99 ;
        temp_card.type_main = 2 ;
        temp_card.type_sub = 1 ;
        RangeSet (-10, 19, temp_card.range_list) ;
        temp_card.damage_aura = -999 ;
        temp_card.damage_life = -999 ;
        temp_card.charge = -999 ;
        temp_card.cost = -999 ;
        temp_card.owner = 0 ;
        A_normal[i++] = temp_card ;
    }
    if (0) {
        Object_Card temp_card = { 0 } ;
        temp_card.classification = 0 ;
        memcpy(temp_card.name, "患い", sizeof(temp_card.name)) ;
        temp_card.cardID = 990101008 ;
        temp_card.goddess_data = 99 ;
        temp_card.type_main = 2 ;
        temp_card.type_sub = 1 ;
        RangeSet (-10, 19, temp_card.range_list);
        temp_card.damage_aura = -999 ;
        temp_card.damage_life = -999 ;
        temp_card.charge = -999 ;
        temp_card.cost = -999 ;
        temp_card.owner = 0 ;
        A_normal[i++] = temp_card ; // 最後に、リストのi番目に代入
    }
    if (0) {
        Object_Card temp_card = { 0 } ;
        temp_card.classification = 0 ;
        memcpy(temp_card.name, "陰の罠", sizeof(temp_card.name)) ;
        temp_card.cardID = 990101009 ;
        temp_card.goddess_data = 99 ;
        temp_card.type_main = 3 ;
        temp_card.type_sub = 0 ;
        RangeSet (-10, 19, temp_card.range_list);
        temp_card.damage_aura = -999 ;
        temp_card.damage_life = -999 ;
        temp_card.charge = 2 ;
        temp_card.cost = -999 ;
        temp_card.owner = 0 ;
        A_normal[i++] = temp_card ; // 最後に、リストのi番目に代入
    }

    // ウツロ・切札群
    i = 0 ;
    if (1) {
        Object_Card temp_card = { 0 } ;
        temp_card.classification = 1 ;
        memcpy(temp_card.name, "数多ノ刃", sizeof(temp_card.name)) ;
        temp_card.cardID = 990102001 ;
        temp_card.goddess_data = 99 ;
        temp_card.type_main = 1 ;
        temp_card.type_sub = 0 ;
        RangeSet (1, 2, temp_card.range_list);
        temp_card.damage_aura = 4 ;
        temp_card.damage_life = 3 ;
        temp_card.charge = -999 ;
        temp_card.cost = 5 ;
        temp_card.owner = 0 ;
        A_ultimate[i++] = temp_card ; // 最後に、リストのi番目に代入
    }
    if (1) {
        Object_Card temp_card = { 0 } ;
        temp_card.classification = 1 ;
        memcpy(temp_card.name, "闇凪ノ声", sizeof(temp_card.name)) ;
        temp_card.cardID = 990102002 ;
        temp_card.goddess_data = 99 ;
        temp_card.type_main = 2 ;
        temp_card.type_sub = 0 ;
        RangeSet (-10, 19, temp_card.range_list);
        temp_card.damage_aura = -999 ;
        temp_card.damage_life = -999 ;
        temp_card.charge = -999 ;
        temp_card.cost = 4 ;
        temp_card.owner = 0 ;
        A_ultimate[i++] = temp_card ; // 最後に、リストのi番目に代入
    }
    if (1) {
        Object_Card temp_card = { 0 } ;
        temp_card.classification = 1 ;
        memcpy(temp_card.name, "苦ノ外套", sizeof(temp_card.name)) ;
        temp_card.cardID = 990102003 ;
        temp_card.goddess_data = 99 ;
        temp_card.type_main = 2 ;
        temp_card.type_sub = 1 ;
        RangeSet (-10, 19, temp_card.range_list);
        temp_card.damage_aura = -999 ;
        temp_card.damage_life = -999 ;
        temp_card.charge = -999 ;
        temp_card.cost = 3 ;
        temp_card.owner = 0 ;
        A_ultimate[i++] = temp_card ; // 最後に、リストのi番目に代入
    }
    if (0) {
        Object_Card temp_card = { 0 } ;
        temp_card.classification = 1 ;
        memcpy(temp_card.name, "奪イノ茨", sizeof(temp_card.name)) ;
        temp_card.cardID = 990102004 ;
        temp_card.goddess_data = 99 ;
        temp_card.type_main = 2 ;
        temp_card.type_sub = 2 ;
        RangeSet (-10, 19, temp_card.range_list);
        temp_card.damage_aura = -999 ;
        temp_card.damage_life = -999 ;
        temp_card.charge = -999 ;
        temp_card.cost = 1 ;
        temp_card.owner = 0 ;
        A_ultimate[i++] = temp_card ; // 最後に、リストのi番目に代入
    }


    // ホノカのデッキも同様に。通常札群
    i = 0 ;
    if (1) {
        Object_Card temp_card = { 0 } ;
        temp_card.classification = 0 ;
        memcpy(temp_card.name, "花弁刃", sizeof(temp_card.name)) ;
        temp_card.cardID = 990201001 ;
        temp_card.goddess_data = 99 ;
        temp_card.type_main = 1 ;
        temp_card.type_sub = 0 ;
        RangeSet (4, 5, temp_card.range_list);
        temp_card.damage_aura = -1 ;
        temp_card.damage_life = 1 ;
        temp_card.charge = -999 ;
        temp_card.cost = -999 ;
        temp_card.owner = 1 ;
        B_normal[i++] = temp_card ; // 最後に、リストのi番目に代入
    }
    if (1) {
        Object_Card temp_card = { 0 } ;
        temp_card.classification = 0 ;
        memcpy(temp_card.name, "桜刀", sizeof(temp_card.name)) ;
        temp_card.cardID = 990201002 ;
        temp_card.goddess_data = 99 ;
        temp_card.type_main = 1 ;
        temp_card.type_sub = 0 ;
        RangeSet (3, 4, temp_card.range_list);
        temp_card.damage_aura = 3 ;
        temp_card.damage_life = 1 ;
        temp_card.charge = -999 ;
        temp_card.cost = -999 ;
        temp_card.owner = 1 ;
        B_normal[i++] = temp_card ; // 最後に、リストのi番目に代入
    }
    if (1) {
        Object_Card temp_card = { 0 } ;
        temp_card.classification = 0 ;
        memcpy(temp_card.name, "瞬霊式", sizeof(temp_card.name)) ;
        temp_card.cardID = 990201003 ;
        temp_card.goddess_data = 99 ;
        temp_card.type_main = 1 ;
        temp_card.type_sub = 0 ;
        RangeSet (5, 5, temp_card.range_list);
        temp_card.damage_aura = 3 ;
        temp_card.damage_life = 2 ;
        temp_card.charge = -999 ;
        temp_card.cost = -999 ;
        temp_card.owner = 1 ;
        B_normal[i++] = temp_card ; // 最後に、リストのi番目に代入
    }
    if (1) {
        Object_Card temp_card = { 0 } ;
        temp_card.classification = 0 ;
        memcpy(temp_card.name, "返し斬り", sizeof(temp_card.name)) ;
        temp_card.cardID = 990201004 ;
        temp_card.goddess_data = 99 ;
        temp_card.type_main = 1 ;
        temp_card.type_sub = 1 ;
        RangeSet (3, 4, temp_card.range_list);
        temp_card.damage_aura = 2 ;
        temp_card.damage_life = 1 ;
        temp_card.charge = -999 ;
        temp_card.cost = -999 ;
        temp_card.owner = 1 ;
        B_normal[i++] = temp_card ; // 最後に、リストのi番目に代入
    }
    if (1) {
        Object_Card temp_card = { 0 } ;
        temp_card.classification = 0 ;
        memcpy(temp_card.name, "歩法", sizeof(temp_card.name)) ;
        temp_card.cardID = 990201005 ;
        temp_card.goddess_data = 99 ;
        temp_card.type_main = 2 ;
        temp_card.type_sub = 0 ;
        RangeSet (-10, 19, temp_card.range_list);
        temp_card.damage_aura = -999 ;
        temp_card.damage_life = -999 ;
        temp_card.charge = -999 ;
        temp_card.cost = -999 ;
        temp_card.owner = 1 ;
        B_normal[i++] = temp_card ; // 最後に、リストのi番目に代入
    }
    if (1) {
        Object_Card temp_card = { 0 } ;
        temp_card.classification = 0 ;
        memcpy(temp_card.name, "桜寄せ", sizeof(temp_card.name)) ;
        temp_card.cardID = 990201006 ;
        temp_card.goddess_data = 99 ;
        temp_card.type_main = 2 ;
        temp_card.type_sub = 1 ;
        RangeSet (-10, 19, temp_card.range_list);
        temp_card.damage_aura = -999 ;
        temp_card.damage_life = -999 ;
        temp_card.charge = -999 ;
        temp_card.cost = -999 ;
        temp_card.owner = 1 ;
        B_normal[i++] = temp_card ; // 最後に、リストのi番目に代入
    }
    if (1) {
        Object_Card temp_card = { 0 } ;
        temp_card.classification = 0 ;
        memcpy(temp_card.name, "光輝収束", sizeof(temp_card.name)) ;
        temp_card.cardID = 990201006 ;
        temp_card.goddess_data = 99 ;
        temp_card.type_main = 2 ;
        temp_card.type_sub = 2 ;
        RangeSet (-10, 19, temp_card.range_list);
        temp_card.damage_aura = -999 ;
        temp_card.damage_life = -999 ;
        temp_card.charge = -999 ;
        temp_card.cost = -999 ;
        temp_card.owner = 1 ;
        B_normal[i++] = temp_card ; // 最後に、リストのi番目に代入
    }
    if (0) {
        Object_Card temp_card = { 0 } ;
        temp_card.classification = 0 ;
        memcpy(temp_card.name, "光の刃", sizeof(temp_card.name)) ;
        temp_card.cardID = 990201008 ;
        temp_card.goddess_data = 99 ;
        temp_card.type_main = 1 ;
        temp_card.type_sub = 0 ;
        RangeSet (3, 5, temp_card.range_list);
        temp_card.damage_aura = -100 ;
        temp_card.damage_life = -1 ;
        temp_card.charge = -999 ;
        temp_card.cost = -999 ;
        temp_card.owner = 1 ;
        B_normal[i++] = temp_card ; // 最後に、リストのi番目に代入
    }
    if (0) {
        Object_Card temp_card = { 0 } ;
        temp_card.classification = 0 ;
        memcpy(temp_card.name, "精霊連携", sizeof(temp_card.name)) ;
        temp_card.cardID = 990201009 ;
        temp_card.goddess_data = 99 ;
        temp_card.type_main = 3 ;
        temp_card.type_sub = 2 ;
        RangeSet (-10, 19, temp_card.range_list);
        temp_card.damage_aura = -999 ;
        temp_card.damage_life = -999 ;
        temp_card.charge = 3 ;
        temp_card.cost = -999 ;
        temp_card.owner = 1 ;
        B_normal[i++] = temp_card ; // 最後に、リストのi番目に代入
    }

    // ホノカ・切札群
    i = 0 ;
    if (1) {
        Object_Card temp_card = { 0 } ;
        temp_card.classification = 1 ;
        memcpy(temp_card.name, "光満ちる一刀", sizeof(temp_card.name)) ;
        temp_card.cardID = 990202001 ;
        temp_card.goddess_data = 99 ;
        temp_card.type_main = 1 ;
        temp_card.type_sub = 0 ;
        RangeSet (3, 4, temp_card.range_list);
        temp_card.damage_aura = 4 ;
        temp_card.damage_life = 3 ;
        temp_card.charge = -999 ;
        temp_card.cost = 5 ;
        temp_card.owner = 1 ;
        B_ultimate[i++] = temp_card ; // 最後に、リストのi番目に代入
    }
    if (1) {
        Object_Card temp_card = { 0 } ;
        temp_card.classification = 1 ;
        memcpy(temp_card.name, "花吹雪の景色", sizeof(temp_card.name)) ;
        temp_card.cardID = 990202002 ;
        temp_card.goddess_data = 99 ;
        temp_card.type_main = 2 ;
        temp_card.type_sub = 0 ;
        RangeSet (-10, 19, temp_card.range_list);
        temp_card.damage_aura = -999 ;
        temp_card.damage_life = -999 ;
        temp_card.charge = -999 ;
        temp_card.cost = 4 ;
        temp_card.owner = 1 ;
        B_ultimate[i++] = temp_card ; // 最後に、リストのi番目に代入
    }
    if (1) {
        Object_Card temp_card = { 0 } ;
        temp_card.classification = 1 ;
        memcpy(temp_card.name, "精霊たちの風", sizeof(temp_card.name)) ;
        temp_card.cardID = 990202003 ;
        temp_card.goddess_data = 99 ;
        temp_card.type_main = 2 ;
        temp_card.type_sub = 1 ;
        RangeSet (-10, 19, temp_card.range_list);
        temp_card.damage_aura = -999 ;
        temp_card.damage_life = -999 ;
        temp_card.charge = -999 ;
        temp_card.cost = 3 ;
        temp_card.owner = 1 ;
        B_ultimate[i++] = temp_card ; // 最後に、リストのi番目に代入
    }
    if (0) {
        Object_Card temp_card = { 0 } ;
        temp_card.classification = 1 ;
        memcpy(temp_card.name, "煌めきの乱舞", sizeof(temp_card.name)) ;
        temp_card.cardID = 990202004 ;
        temp_card.goddess_data = 99 ;
        temp_card.type_main = 1 ;
        temp_card.type_sub = 0 ;
        RangeSet (3, 5, temp_card.range_list);
        temp_card.damage_aura = 2 ;
        temp_card.damage_life = 2 ;
        temp_card.charge = -999 ;
        temp_card.cost = 2 ;
        temp_card.owner = 1 ;
        B_ultimate[i++] = temp_card ; // 最後に、リストのi番目に代入
    }

}

// 総合ルール4-1　桜花決闘の準備　を行う関数
void PreparForDuel ( Object_Card A_normal[], Object_Card A_ultimate[], Object_Card B_normal[], Object_Card B_ultimate[] )
{
    // 最初に、オーラ上限などの定数を代入
    areas.area_aura_A.limit_upper = 5 ;
    areas.area_aura_B.limit_upper = 5 ;
    other_status.player_status[0].vigor_limit_upper = 2 ;
    other_status.player_status[1].vigor_limit_upper = 2 ;
    other_status.player_status[0].hand_limit_upper  = 2 ;
    other_status.player_status[1].hand_limit_upper  = 2 ;
    other_status.master_distance = 2 ;

    // ルール4-1-1
    areas.area_distance.placed_SAKURA = 10 ;
    areas.area_aura_A.placed_SAKURA = 3 ;
    areas.area_aura_B.placed_SAKURA = 3 ;
    areas.area_life_A.placed_SAKURA = 10 ;
    areas.area_life_A.placed_SAKURA = 10 ;

    // ルール4-1-2
    for ( int i=0; i<7; i++ ) {
        areas.area_deck_A.cards_stack[i] = A_normal[i] ;
        areas.area_deck_B.cards_stack[i] = B_normal[i] ;
    }
    for ( int i=0; i<3; i++ ) {
        areas.area_ultimate_A.cards_stack[i] = A_ultimate[i] ;
        areas.area_ultimate_B.cards_stack[i] = B_ultimate[i] ;
    }
    areas.area_deck_A.number_cards = 7 ;
    areas.area_deck_B.number_cards = 7 ;
    areas.area_ultimate_A.number_cards = 3 ;
    areas.area_ultimate_B.number_cards = 3 ;
    // 以上で通常札と切札を所定の位置に配置完了


    // ルール4-1-3
    // これは後で良い


    // ルール4-1-4　デッキをシャッフルして３ドロー
    ShuffleDeck (0) ;
    ShuffleDeck (1) ;
    DrawCard (0) ;
    DrawCard (0) ;
    DrawCard (0) ;
    DrawCard (1) ;
    DrawCard (1) ;
    DrawCard (1) ;

    // ルール4-1-5　マリガン
    // インタラクションが必要なので後回しにしたい……

    // ルール4-1-6
    other_status.player_status[1].vigor = 1 ;
    
    // ルール4-1-7　スタックポインタの１つ目・始点となる「次のターンの開始処理」を積む
    Operation temp_ope = { -1, 1, ope_IDused++, 0, {0} } ;
    ope_stack[0] = temp_ope ;
    other_status.turn_player = 1 ;
}

// ルール5-2-1、「現在の間合」を計算する
int CalcNowDistance ()
{
    return areas.area_distance.placed_SAKURA ;
}

// 「ある領域」に「このカード」はありますか？という関数。あればTRUE
bool IsThereCard ( Object_Card list[], int cardID, int cardnum )
{
    for (int i=0; i<cardnum ; i++ ) {
        if ( list[i].cardID == cardID ) 
            return true;
    }
    return false;
}

// 山札にあるカードをシャッフルする関数
void ShuffleDeck ( int subject )
{
    int number ;
    Object_Card* deck ;
    // デッキとその枚数を表す変数に、主観に応じて参照を渡す
    if (subject==0) {
        deck = areas.area_deck_A.cards_stack ;
        number = areas.area_deck_A.number_cards ;
    }
    else {
        deck = areas.area_deck_B.cards_stack ;
        number = areas.area_deck_B.number_cards ;
    }   // 前処理、終了

    // 山札の枚数が0か1なら何もせずreturn
    if (number==0 || number==1)
        return;

    // ここから本題
    for ( int i=0; i<400; i++ ) {
        // 山札のX枚目とY枚目を入れ替える
        int X = genrand_int32() % number ;
        int Y = genrand_int32() % number ;

        Object_Card temp = deck[Y] ;
        deck[Y] = deck[X] ;
        deck[X] = temp ;
    }
}

// 「カードを１枚引く」を行う関数。山札があるときだけ動かすように！
void DrawCard ( int subject )
{
    // 主観に応じて参照を渡す
    Area_Deck_Personal* deck = PA[subject].area_deck ;
    Area_Hand_Personal* hand = PA[subject].area_hand ;
    int decknum = deck->number_cards ;
    int handnum = hand->number_cards ;

    // 手札の最後に、デッキトップのデータをコピー
    hand->cards_stack[handnum] = deck->cards_stack[decknum-1] ;
    // デッキトップのデータをクリア
    deck->cards_stack[decknum-1] = object_card_formatdata ;
    // 手札の枚数を１増やす、山札の枚数を１減らす
    hand->number_cards += 1 ;
    deck->number_cards -= 1 ;
};

// ルール10-4-1、距離拡大（近Ｘ）を適用する。引数は攻撃オブジェクトとＸ
Object_Attack RangeExpansionNear ( Object_Attack atk, int n )
{
    int i = 0 ;     // 最小の間合いのインデックスを探す用
    while ( atk.range_list[i]==false && i<30 ) {
        i++ ;
    }
    if ( i == 30 )   // 適正距離が空集合だったなら
        return atk ;
    
    while ( 1<i && 0<n ) {
        atk.range_list[--i] = true ;
        n-- ;
    }
    return atk ;
}




// 盤面状況を（視点に応じて）出力する関数。インタラクションの際につかう
void InteractionBoardOutput ( int subject )
{
    int opponent = 1-subject ;

    printf ( "――――――――――――――――――――――――――――――――――\n\n" ) ;
    OpponentUltimateOutput (subject) ;  // 相手の切札
    OpponentPlayedOutput (subject) ;   // 相手の捨て札
    printf ( "　　　　　手札：%-2d　　　伏札：%-2d\n\n", PA[opponent].area_hand->number_cards, PA[opponent].area_discard->number_cards ) ;
    OpponentEnchantOutput (subject) ;
    printf ( "　　ライフ：%-2d　オーラ：%-2d　集中力：%-2d　山札：%-2d\n", PA[opponent].area_life->placed_SAKURA, PA[opponent].area_aura->placed_SAKURA, other_status.player_status[opponent].vigor, PA[opponent].area_deck->number_cards );
    printf ( "　　　フレア：%-2d\n", PA[opponent].area_flare->placed_SAKURA );
    printf ( "　　　　　　間合い：%-2d　　ダスト：%-2d\n", areas.area_distance.placed_SAKURA, areas.area_shadow.placed_SAKURA );
    printf ( "　　　　　　　　　　　　　　　　フレア：%-2d\n", PA[subject].area_flare->placed_SAKURA );
    printf ( "山札：%-2d　集中力：%-2d　オーラ：%-2d　ライフ：%-2d\n", PA[subject].area_deck->number_cards, other_status.player_status[subject].vigor, PA[subject].area_aura->placed_SAKURA, PA[subject].area_life->placed_SAKURA );
    SubjectEnchantOutput (subject) ;
    printf ( "\n" ) ;
    SubjectHandOutput (subject) ;  // 自分の手札
    SubjectUltimateOutput (subject) ;   // 自分の切札
    SubjectPlayedOutput (subject) ;     // 自分の捨て札
    SubjectDiscardOutput (subject) ;    // 自分の伏せ札

    printf ( "\n" ) ;
}

// 視点プレイヤーから見た対戦相手の切札を出力
void OpponentUltimateOutput ( int subject )
{
    int opponent = 1-subject ;
    printf ( "　　　　　切札：" ) ;
    for ( int i=0 ; i<PA[opponent].area_ultimate->number_cards ; i++ )
    {
        if ( PA[opponent].area_ultimate->cards_stack[i].face_up == false ) {
            printf ( " 裏 ," ) ;
        }
        else {
            printf ( " %s ,", PA[opponent].area_ultimate->cards_stack[i].name ) ;
        }
    }
    printf ( "\n" ) ;
}

// 視点プレイヤーから見た対戦相手の捨札を出力
void OpponentPlayedOutput ( int subject )
{
    int opponent = 1-subject ;
    printf ( "　　　　　捨札：" ) ;
    for ( int i=0 ; i<PA[opponent].area_played->number_cards ; i++ )
    {
        printf ( " %s ,", PA[opponent].area_played->cards_stack[i].name ) ;
    }
    printf ( "\n" ) ;
}

// 視点プレイヤーから見た対戦相手の付与札を出力
void OpponentEnchantOutput (int subject)
{
    int opponent = 1-subject ;
    printf ( "付与札：" ) ;
    for ( int i=0 ; i<PA[opponent].area_enchant->number_cards ; i++ )
    {
        printf ( " %s ,", PA[opponent].area_enchant->cards_stack[i].name ) ;
    }
    printf ( "\n" ) ;
}

// 視点プレイヤーから見た自分の付与札を出力
void SubjectEnchantOutput (int subject)
{
    printf ( "付与札：" ) ;
    for ( int i=0 ; i<PA[subject].area_enchant->number_cards ; i++ )
    {
        printf ( " %s ,", PA[subject].area_enchant->cards_stack[i].name ) ;
    }
    printf ( "\n" ) ;
}

// 視点プレイヤーから見た自分の手札を出力
void SubjectHandOutput ( int subject )
{
    printf ( "手札：" ) ;
    for ( int i=0 ; i<PA[subject].area_hand->number_cards ; i++ )
    {
        printf ( " %s ,", PA[subject].area_hand->cards_stack[i].name ) ;
    }
    printf ( "\n" ) ;
}

// 視点プレイヤーから見た自分の切札を出力
void SubjectUltimateOutput (int subject)
{
    printf ( "切札：" ) ;
    for ( int i=0 ; i<PA[subject].area_ultimate->number_cards ; i++ )
    {
        if ( PA[subject].area_ultimate->cards_stack[i].face_up == false ) {
            printf ( " (裏) %s ,", PA[subject].area_ultimate->cards_stack[i].name ) ;
        }
        else {
            printf ( " (表) %s ,", PA[subject].area_ultimate->cards_stack[i].name ) ;
        }
    }
    printf ( "\n" ) ;
}

// 視点プレイヤーから見た自分の捨て札を出力
void SubjectPlayedOutput (int subject)
{
    printf ( "捨札：" ) ;
    for ( int i=0 ; i<PA[subject].area_played->number_cards ; i++ )
    {
        printf ( " %s ,", PA[subject].area_played->cards_stack[i].name ) ;
    }
    printf ( "\n" ) ;
}

// 視点プレイヤーから見た自分の伏せ札を出力
void SubjectDiscardOutput (int subject)
{
    printf ( "伏札：" ) ;
    for ( int i=0 ; i<PA[subject].area_discard->number_cards ; i++ )
    {
        printf ( " %s ,", PA[subject].area_discard->cards_stack[i].name ) ;
    }
    printf ( "\n" ) ;
}




// 「手札か切札からカードを１枚使用」「基本動作を行う」「何もせずエンド」から選ばせる。（全力と基本動作はそれぞれ可能ならtrueを受け取る）
int UseCardOrBasicAction (int CON, bool fullpower, bool basicaction)
{
    int choice;

    TOP_OF_FUNCTION :   // gotoのためのラベル

    // 基本動作も可な場合のみ通過する。不可だとそのまま下記のUSE_CARDへ行く
    if ( basicaction ) {
        printf ( "question：基本動作をしますか、カードの使用をしますか\n" ) ;
        printf ( "option  ： 1-基本動作 , 2-カード使用\n" ) ;
        printf ( "answer? ：" ) ;
        scanf("%d", &choice );
        while (getchar() != '\n');  // 直後に、余分な入力分を破棄させる。

        if (choice==1) {        // 基本動作をしたい
            return AskBasicAction (CON, false) ;
        }
        else if (choice==2) {   // カードの使用をしたい
            goto USE_CARD ;
        }
        else {
            printf ( "不正な入力です。\n" ) ;
            goto TOP_OF_FUNCTION ;
        }
    }   

    USE_CARD :
    {   // １つ下のスコープにしておくことでデータをローカル化
    int correct_list[20] = {0} ;
    int list_num = 1 ;  // リストの要素１つめは0で良いので、numは１から
    printf ( "question：どのカードを使用しますか？\n" ) ;
    printf ( "option  ： 0-なにも選択せずエンド  ,  50-最初に戻る\n" ) ;

    // 手札に関する出力
    printf ( "option  ：手札： " ) ;
    for (int i=0; i<PA[CON].area_hand->number_cards ; i++) {
        // 使用可能と判断されたカード１つずつ順に
        if ( AvailableCards ( PA[CON].area_hand->cards_stack[i], CON, fullpower ) )  {
            printf ( "%d-%s  ,  ", 100+i , PA[CON].area_hand->cards_stack[i].name ) ;
            correct_list[list_num++] = 100+i ;
        }
    }
    printf ( "\n" ) ;

    // 切札に関する出力
    printf ( "option  ：切札： " ) ;
    for (int i=0; i<PA[CON].area_ultimate->number_cards ; i++) {
        if ( AvailableCards ( PA[CON].area_ultimate->cards_stack[i], CON, fullpower ) )  {
            printf ( "%d-%s  ,  ", 200+i , PA[CON].area_ultimate->cards_stack[i].name ) ;
            correct_list[list_num++] = 200+i ;
        }
    }
    printf ( "\n" ) ;

    // 入力を受け付け、適正だったらそれをreturn
    printf ( "answer? ：" ) ;
    int choice = -1;
    while ( 1 )  {
        scanf("%d", &choice );
        while (getchar() != '\n');  // 直後に、余分な入力分を破棄させる。

        if ( choice==50 )   // 最初に戻る、を選択した場合
            goto TOP_OF_FUNCTION ;
        
        for (int i=0; i<list_num ; i++) {   // collectlistに該当する数字があればそれをreturnして終了
            if ( correct_list[i]==choice )
                return choice ;
        }
        printf ( "不正な入力です。再度入力して下さい：" ) ;
    }
    }
}

// どの基本動作がしたいかプレイヤーに質問。mustがtrueだと「しない」ことが選べない。
int AskBasicAction (int CON, bool must)
{
    // それぞれの基本動作について可能か否かを保存する配列。 なし・前進・後退・纏い・宿し・離脱　の順
    bool collect_list[10] = {0} ;

    printf ( "question：どの基本動作をしますか？\n" ) ;
    if ( !must ) {
        printf ( "option  ： 0-なにも選択せずエンド\n" ) ;
        collect_list[0] = true ;
    }
    
    printf ( "option  ： " ) ;
    if ( CanAdvance(CON) ) {
        printf ( "1-前進 , " ) ;
        collect_list[1] = true ;
    }
    if ( CanRetreat(CON) ) {
        printf ( "2-後退 , " ) ;
        collect_list[2] = true ;
    }
    if ( CanRecover(CON) ) {
        printf ( "3-纏い , " ) ;
        collect_list[3] = true ;
    }
    if ( CanFocus(CON) ) {
        printf ( "4-宿し , " ) ;
        collect_list[4] = true ;
    }
    if ( CanBreakaway(CON) ) {
        printf ( "5-離脱 , " ) ;
        collect_list[5] = true ;
    }
    printf ( "\n" ) ;

    // 入力を受け付け、適正だったらそれをreturn
    printf ( "answer? ：" ) ;
    int choice = -1;
    while ( 1 )  {
        scanf("%d", &choice );
        while (getchar() != '\n');  // 直後に、余分な入力分を破棄させる。

        if ( -1 < choice && choice < 10 )   // 配列領域外を参照しないための保険
            if ( collect_list[choice] == true )
                return choice ;

        printf ( "不正な入力です。再度入力して下さい：" ) ;
    }
}

// 基本動作のコストを尋ねる。ルール9-6-ii より、opecode7に固有と思われるのでそれ専用の書き方にする
int AskPayBACost (int CON)
{
    bool collect_list[20] = {0} ;

    printf ( "question：何をコストにしますか？\n" ) ;
    if ( 0 < other_status.player_status[CON].vigor )
        printf ( "option  ： 1-集中力  ,  50-最初に戻る\n" ) ;
    else {
        printf ( "option  ： 50-最初に戻る\n" ) ;
    }
    printf ( "option  ：手札： " ) ;

    for (int i=0; i<PA[CON].area_hand->number_cards ; i++) {
        if ( 1 ) {  // それが伏せ札にできるカードなら。なお始まりの決闘だけなら、必ずできる。
            printf ( "%d-%s  ,  ", 100+i , PA[CON].area_hand->cards_stack[i].name ) ;
            collect_list[i] = true ;
        }
    }

    // 入力を受け付け、適正だったらそれをreturn
    printf ( "\nanswer? ：" ) ;
    int choice = -1;
    while ( 1 )  {
        scanf("%d", &choice );
        while (getchar() != '\n');  // 直後に、余分な入力分を破棄させる。

        if ( (choice==1&&0<other_status.player_status[CON].vigor) || choice==50 )
            return choice ;

        if ( 99 < choice && choice < 120 )   // 配列領域外を参照しないための保険
            if ( collect_list[choice-100] == true )
                return choice ;

        printf ( "不正な入力です。再度入力して下さい：" ) ;
    }

}




// 攻撃カードを使用した場合に生成されるであろう《攻撃》を推定する（ゴーストアタックとして生成）
Object_Attack MakeGhostAttackObject ( Object_Card atkcard, int CON )
{
    Object_Attack ghost = {0} ;
    ghost.classification = atkcard.classification ;
    ghost.type_sub = atkcard.type_sub ;
    memcpy(ghost.range_list, atkcard.range_list, sizeof(ghost.range_list));
    ghost.damage_aura = atkcard.damage_aura ;
    ghost.damage_life = atkcard.damage_life ;

    // エフェクトトークンズによる効果を加味
    if ( 0 < other_status.player_status[CON].tokens.kihaku ) {
        ghost = RangeExpansionNear ( ghost, 1 ) ;
    }

    return ghost ;
}

// あるcardについて、現時点で使用可能かを判定。可能trueか、不可能falseを返す。引数フルパワーがfalseなら全力カードはダメ。
bool AvailableCards ( Object_Card card , int CON, bool fullpower)
{
    // そのカードが攻撃の場合、ゴーストを作って適正距離を確認
    if ( card.type_main == 1 ) {  
        Object_Attack ghost ;
        ghost = MakeGhostAttackObject (card, CON) ;

        // 作成したゴーストの適正距離に現在の間合いが入っていなければfalse
        if ( ghost.range_list[CalcNowDistance()+10] == false )
            return false ;
    }

    // 全力使用NGかつ、そのカードが全力札ならfalse
    if ( fullpower==false  &&  card.type_sub==2  ) {
        return false ;
    }
    
    // すべての判定を回避したらtrue
    return true ;
}

// 基本動作のコストが払えそうかどうかを判定。払えるならtrue
bool CanPayBACost (int CON)
{
    if ( 0 < other_status.player_status[CON].vigor )
        return true ;
    if ( 0 < PA[CON].area_hand->number_cards )
        return true ;

    return false ;
}

// 前進で何らかのオブジェクトが移動するかチェック。移動するならtrue
bool CanAdvance (int CON)
{
    if ( areas.area_distance.placed_SAKURA == 0 )
        return false ;
    if ( CalcNowDistance() <= other_status.master_distance )
        return false ;
    if ( PA[CON].area_aura->limit_upper <= (PA[CON].area_aura->placed_SAKURA+PA[CON].area_aura->placed_FREEZE) )
        return false ;

    return true ;
}

// 後退で何らかのオブジェクトが移動するかチェック。移動するならtrue
bool CanRetreat (int CON)
{
    if ( 10 <= areas.area_distance.placed_SAKURA )
        return false ;
    if ( PA[CON].area_aura->placed_SAKURA < 1 )
        return false ;

    return true ;
}

// 纏いで何らかのオブジェクトが移動するかチェック。移動するならtrue
bool CanRecover (int CON)
{
    if ( areas.area_shadow.placed_SAKURA < 1 )
        return false ;
    if ( PA[CON].area_aura->limit_upper <= (PA[CON].area_aura->placed_SAKURA+PA[CON].area_aura->placed_FREEZE) )
        return false ;

    return true ;
}

// 宿しで何らかのオブジェクトが移動するかチェック。移動するならtrue
bool CanFocus (int CON)
{
    if ( PA[CON].area_aura->placed_SAKURA < 1 )
        return false ;

    return true ;
}

// 離脱で何らかのオブジェクトが移動するかチェック。移動するならtrue
bool CanBreakaway (int CON)
{
    if ( areas.area_shadow.placed_SAKURA < 1 )
        return false ;
    if ( 10 <= areas.area_distance.placed_SAKURA )
        return false ;
    if ( other_status.master_distance < CalcNowDistance() )
        return false ;

    return true ;
}

// 「基本動作XXを行う」のオペレーションを返す。XXはintで1:前進から初めて、後退,纏い,宿し,離脱。
// 前進～離脱はopecodeが連番なので定数でも足せば良いが、追加基本動作とかで必要になると思うので
int ReBAOpeCode ( int actionnum )
{
    if ( 0 < actionnum && actionnum < 6 ) {
        // 前進～離脱なら、13足せばopeco
        return actionnum+13 ;
    }
    return 9999001 ;
}




// オペレーションスタックにpushする
void push (Operation newope)
{
    ope_stack[++ope_number] = newope ;
}

// オペレーションスタックをクリアする
void StackRefresh ()
{
    for (int i=0; i<5000; i++)
        ope_stack[i] = operation_formatdata ;
}

// 内部処理用。適正距離X-Yをlistに格納する。
void RangeSet ( int min, int max, bool rangelist[] )
{
    if ( min < -10 )    // minが-10以下の場合アンダーフローしかねないので-10に丸める
        min = -10 ;
    if ( 19 < max )     // maxが19以上の場合オーバーフローしかねないので19に丸める
        max = 19 ;
    if ( max < min )   // max < min ならば
        return ;

    min += 10 ;
    max += 11 ;
    for (int i=min; i<max; i++)
        rangelist[i] = true ;
}

// 内部処理用。カードスタックの空いた場所を詰めて、最後尾をフォーマットして、カード枚数を１減らす。　カードがスタック最後尾以外から移動したときに使う。
void SortCardStack ( Object_Card card_stack[], int position, int* cardnum )
{
    for (int i=position+1 ; i < *cardnum ; i++)
        card_stack[i-1] = card_stack[i] ;
    
    card_stack[*cardnum] = object_card_formatdata ;
    (*cardnum) -- ;
};













/* 一応mainということになっている関数。乱数の設定、眼前構築と決闘の準備を行う。
その後に桜花決闘の処理に入り、主にこれに重点をおいている関数である */
int main ()
{
    // ログ出力ファイルを開ける
    FILE *fp;
    fp = fopen("HBBC_log.txt","w");

    // 現在の時刻をシードとして使用してメルセンヌツイスタを初期化
    long seed = time(NULL) ;
    fprintf (fp, "シード：%ld\n", seed);
    init_genrand( seed );



    // 両者が眼前構築で選んだ通常札と切札を格納する配列
    Object_Card A_normal[7] = {0};
    Object_Card A_ultimate[3] = {0};
    Object_Card B_normal[7] = {0};
    Object_Card B_ultimate[3] = {0};
    
    // 両者の通常札と切札を選択、配列に直で書き込ませる
    DeckOfBeginningDuel (A_normal, A_ultimate, B_normal, B_ultimate );

    // 「桜花決闘の準備」をさせる
    PreparForDuel (A_normal, A_ultimate, B_normal, B_ultimate );


    // テスト用
    // other_status.player_status[0].tokens.kihaku = 1;


    int Game_End_Flag = 0 ;
    int roop_counter = 0 ;  // 無限ループになっていそうな時に終了させる用

    // メインとなる、オペレーションループ
    while ( Game_End_Flag==0 ) {
        roop_counter ++;

        // 異常事態用
        if ( ope_number < 0 ) {
            printf("フェイタルエラー：スタックポインタの値がマイナスです\n") ;
            fprintf(fp, "フェイタルエラー：スタックポインタの値がマイナスです\n") ;
            printf("強制終了します\n") ;
            goto FATAL_ERROR_END ;
        }
        if ( 4999 < ope_number ) {
            printf("フェイタルエラー：スタックポインタの値がオーバーフローしています\n") ;
            fprintf(fp, "フェイタルエラー：スタックポインタの値がオーバーフローしています\n") ;
            printf("強制終了します\n") ;
            goto FATAL_ERROR_END ;
        }
        if ( 10000 < roop_counter ) {
            printf("セーフティ：処理が無限ループしている可能性があります\n") ;
            fprintf(fp, "セーフティ：処理が無限ループしている可能性があります\n") ;
            printf("強制終了します\n") ;
            goto FATAL_ERROR_END ;
        }

        // スタックのトップをpopするような処理
        Operation processing = ope_stack[ope_number] ;
        ope_stack[ope_number--] = operation_formatdata ;

        // ターンプレイヤーをTurnP、オペレーションコントローラーをCONと省略
        int TurnP = other_status.turn_player ;
        int CON = processing.controller ;
        
        switch ( processing.ope_code )      // 取り出したスタックの中身に応じて何かを行う
        {
            case 1: {   // ターン開始処理なら
            fprintf (fp, "\n\n\n00001:%06d:%d:ターン開始処理\n", processing.uniqueID, CON );
            printf ( "\n――――――――――――――――――――――――――――――――――\n\n" ) ;
            printf ( "次のターンを開始します。　ターンプレイヤーが相手に交代します。\n\n" ) ;
            printf ( "――――――――――――――――――――――――――――――――――\n\n" ) ;
            other_status.turn_number ++ ;   // ターン数を+1
            other_status.turn_player = !other_status.turn_player ; // ターンプレイヤーを交代
            TurnP = other_status.turn_player ;  // ターンプレイヤー情報を書き換えたので。
            StackRefresh() ;    // ターン開始時なのでスタックを大掃除してもいいはず？
            roop_counter = 0 ;  // スタックリセットするならターン跨いで無限ループはしないはずなので数え直し

            Operation temp2 = {TurnP, 2, ope_IDused++, processing.uniqueID, {0} } ;
            Operation temp3 = {TurnP, 3, ope_IDused++, processing.uniqueID, {0} } ;
            Operation temp4 = {TurnP, 4, ope_IDused++, processing.uniqueID, {0} } ;
            Operation temp1 = {-1, 1, ope_IDused++, processing.uniqueID, {0} } ;
            push( temp1 );   // 全てのフェイズを完了したら、「次のターンを開始する」
            push( temp4 );   // 終了フェイズ開始処理をプッシュ
            push( temp3 );   // メインフェイズ開始処理をプッシュ
            push( temp2 );   // 開始フェイズ開始処理をプッシュ
            } break;


            case 2: {   // 開始フェイズ開始処理なら
            fprintf (fp, "%05d:%06d:%d:開始フェイズ開始処理\n", processing.ope_code, processing.uniqueID, CON );
            // other_status.distance_turnstart =    // ターン開始時の間合い決定処理
            // push( 1001 );       //開始フェイズに◯◯する効果解決時間ですよプッシュ（付与札などに、この処理に反応させてプッシュさせるため）　番号仮置き。後半に持っていく。
            
            if ( 2 < other_status.turn_number ) {
                Operation temp5 = {CON, 5, ope_IDused++, processing.uniqueID, {0} } ;
                push( temp5 );   // 3ターン目以降なら開始フェイズ既定処理をプッシュ
            }
            } break;


            case 3: {   // メインフェイズ開始処理
            fprintf (fp, "%05d:%06d:%d:メインフェイズ開始処理\n", processing.ope_code, processing.uniqueID, CON );
            //メインフェイズに◯◯する効果解決時間ですよプッシュ

            InteractionBoardOutput (CON) ;
            printf ( "question：全力行動しますか？\noption  ：1-全力行動 , 2-標準行動\nanswer? ：" ) ;
            
            int choice;
            OPE3_CHOICE_1:
            choice = 0 ;
            scanf("%d", &choice );
            while (getchar() != '\n');  // 直後に、余分な入力分を破棄させる。
            if (choice==1) {        // 全力行動する
                Operation temp6  = {CON, 6, ope_IDused++, processing.uniqueID, {0} } ;
                push( temp6 );
            }
            else if (choice==2) {   // 標準行動する
                Operation temp7  = {CON, 7, ope_IDused++, processing.uniqueID, {0} } ;
                push( temp7 );
            }
            else {
                printf ( "不正な入力です。再度入力して下さい：" ) ;
                goto OPE3_CHOICE_1 ;
            }
            } break;


            case 4: {   // 終了フェイズ開始処理
            fprintf (fp, "%05d:%06d:%d:終了フェイズ開始処理\n", processing.ope_code, processing.uniqueID, CON );
            // push( 1001 );       //終了フェイズに◯◯する効果解決時間ですよプッシュ（付与札などに、この処理に反応させてプッシュさせるため）　番号仮置き。後半に持っていく。
            
            // 手札上限を超えていたらディスカしろ、をプッシュ
            } break;


            case 5: {   // 開始フェイズ規定処理
            fprintf (fp, "%05d:%06d:%d:開始フェイズ既定処理\n", processing.ope_code, processing.uniqueID, CON );
            Operation temp9  = {CON, 9, ope_IDused++, processing.uniqueID, {0} } ;
            Operation temp10 = {CON, 10, ope_IDused++, processing.uniqueID, {0} } ;
            Operation temp11 = {CON, 11, ope_IDused++, processing.uniqueID, {0} } ;
            Operation temp12 = {CON, 12, ope_IDused++, processing.uniqueID, {0} } ;
            push( temp12 );   // 「カードを１枚引く」
            push( temp12 );   // 「カードを１枚引く」
            push( temp11 );   // 再構成してもよい。
            push( temp10 );   // 付与札カウントダウン処理
            push( temp9 );   // 集中力+１
            } break;


            case 6: {   // 全力行動を行う
            fprintf (fp, "%05d:%06d:%d:全力行動を行う\n", processing.ope_code, processing.uniqueID, CON );
            InteractionBoardOutput (CON) ;
            int usecard = UseCardOrBasicAction (CON, true, false ) ;

            Operation temp13 = {CON, 13, ope_IDused++, processing.uniqueID, {0} } ;
            if ( usecard/100 == 1 ) {       // 通常札ならば
                temp13.detail.cards_stack = PA[CON].area_hand->cards_stack ;
                temp13.detail.card_position = usecard % 100 ;
                temp13.detail.process_cardID = PA[CON].area_hand->cards_stack[usecard%100].cardID ;
                temp13.detail.sauce_cards_num = &(PA[CON].area_hand->number_cards) ;
                push( temp13 );   // 「選んだカードを使用する」
            }
            else if ( usecard/100 == 2 ) {  // 切札ならば
                temp13.detail.cards_stack = PA[CON].area_ultimate->cards_stack ;
                temp13.detail.card_position = usecard % 100 ;
                temp13.detail.process_cardID = PA[CON].area_ultimate->cards_stack[usecard%100].cardID ;
                temp13.detail.sauce_cards_num = &(PA[CON].area_ultimate->number_cards) ;
                push( temp13 );   // 「選んだカードを使用する」
            }
            } break;


            case 7: {   // 標準行動を行う
            fprintf (fp, "%05d:%06d:%d:標準行動を行う\n", processing.ope_code, processing.uniqueID, CON );
            InteractionBoardOutput (CON) ;
            int BA_or_card ;

            OPE7_CHOICE_TOP :
            
            if ( CanPayBACost(CON) )
                BA_or_card = UseCardOrBasicAction (CON, false, true ) ;
            else
                BA_or_card = UseCardOrBasicAction (CON, false, false ) ;

            if ( BA_or_card == 0 ) {;}       // 何もしない択ならなにもしない
            else if ( 0 < BA_or_card && BA_or_card < 6 ) {      // 基本動作なら
                int cost = AskPayBACost (CON) ;
                if ( cost==50 )
                    goto OPE7_CHOICE_TOP ;
                else if ( cost==1 ) {   // 集中力消費を選んだなら
                    Operation tempBAcost = {CON, 8, ope_IDused++, processing.uniqueID, {0} } ;
                    Operation tempBA = {CON, ReBAOpeCode(BA_or_card), ope_IDused++, processing.uniqueID, {0} } ;
                    Operation temp7 = {CON, 7, ope_IDused++, processing.uniqueID, {0} } ;
                    push( temp7 );      // 再帰的に自己をスタックに入れる。
                    push( tempBA );     // 選ばれた基本動作を行う
                    push( tempBAcost );     // 選ばれたコストの支払いを行う
                }
                else {  // 手札を伏せることを選んだなら、
                    Operation tempBAcost = {CON, 19, ope_IDused++, processing.uniqueID, {0} } ;
                    tempBAcost.detail.cards_stack = PA[CON].area_hand->cards_stack ;
                    tempBAcost.detail.card_position = cost % 100 ;
                    tempBAcost.detail.process_cardID = PA[CON].area_hand->cards_stack[cost%100].cardID ;
                    tempBAcost.detail.sauce_cards_num = &(PA[CON].area_hand->number_cards) ;
                    Operation tempBA = {CON, ReBAOpeCode(BA_or_card), ope_IDused++, processing.uniqueID, {0} } ;
                    Operation temp7 = {CON, 7, ope_IDused++, processing.uniqueID, {0} } ;
                    push( temp7 );      // 再帰的に自己をスタックに入れる。
                    push( tempBA );     // 選ばれた基本動作を行う
                    push( tempBAcost );     // 選ばれたコストの支払いを行う
                }
            }
            else if ( BA_or_card/100 == 1 ) {       // 通常札使用ならば
                Operation temp13 = {CON, 13, ope_IDused++, processing.uniqueID, {0} } ;
                Operation temp7 = {CON, 7, ope_IDused++, processing.uniqueID, {0} } ;
                temp13.detail.cards_stack = PA[CON].area_hand->cards_stack ;
                temp13.detail.card_position = BA_or_card % 100 ;
                temp13.detail.process_cardID = PA[CON].area_hand->cards_stack[BA_or_card%100].cardID ;
                push( temp7 );      // 再帰的に自己をスタックに入れる。
                push( temp13 );   // 選ばれたカードを使用する
            }
            else if ( BA_or_card/100 == 2 ) {       // 切札使用ならば
                Operation temp13 = {CON, 13, ope_IDused++, processing.uniqueID, {0} } ;
                Operation temp7 = {CON, 7, ope_IDused++, processing.uniqueID, {0} } ;
                temp13.detail.cards_stack = PA[CON].area_ultimate->cards_stack ;
                temp13.detail.card_position = BA_or_card % 100 ;
                temp13.detail.process_cardID = PA[CON].area_ultimate->cards_stack[BA_or_card%100].cardID ;
                push( temp7 );      // 再帰的に自己をスタックに入れる。
                push( temp13 );   // 選ばれたカードを使用する
            }
            } break;


            case 8: {   // 集中力を１失う
            fprintf (fp, "%05d:%06d:%d:集中力を１失う\n", processing.ope_code, processing.uniqueID, CON );
            // 集中力が１以上なら、１減らす
            if ( 0 < other_status.player_status[CON].vigor )
                other_status.player_status[CON].vigor -- ;
            } break;


            case 9: {   //　集中力を１得る
            fprintf (fp, "%05d:%06d:%d:集中力を１得る\n", processing.ope_code, processing.uniqueID, CON );
            // 畏縮しているなら解除する
            if ( other_status.player_status[CON].stun == true ) {
                other_status.player_status[CON].stun = false ;
            }
            // 畏縮していなければ、集中力上限を上回らない場合に限り、集中力+1
            else if ( other_status.player_status[CON].vigor < other_status.player_status[CON].vigor_limit_upper ) {
                other_status.player_status[CON].vigor ++ ;
            }
            } break;


            case 10: {   // 付与札カウントダウン処理
            fprintf (fp, "%05d:%06d:%d:付与札カウントダウン処理\n", processing.ope_code, processing.uniqueID, CON );
            } break;


            case 11: {   // 再構成してもよい
            fprintf (fp, "%05d:%06d:%d:再構成してもよい\n", processing.ope_code, processing.uniqueID, CON );
            InteractionBoardOutput (CON) ;
            printf ( "question：再構成しますか？\n" ) ;
            printf ( "option  ： 1-する  ,  2-しない\n" ) ;
            printf ( "answer? ：" ) ;
            int choice = 0;
            while ( 1 )  {
                scanf("%d", &choice );
                while (getchar() != '\n');  // 直後に、余分な入力分を破棄させる。

                if ( choice==2 )   // しない、を選択した場合
                    break ;
                else if ( choice==1 ) {     // する、を選択した場合
                    Operation temp20 = {CON, 20, ope_IDused++, processing.uniqueID, {0} } ;
                    Operation temp21 = {CON, 21, ope_IDused++, processing.uniqueID, {0} } ;
                    push( temp21 );   // 再構成を行う
                    push( temp20 );   // 自身のライフに１ダメージ
                    break ;
                }
                else {
                    printf ( "不正な入力です。再度入力して下さい：" ) ;
                }
            }
            } break;


            case 12: {   // 「カードを１枚引く」
            fprintf (fp, "%05d:%06d:%d:「カードを１枚引く」\n", processing.ope_code, processing.uniqueID, CON );
            // 山札が０なら焦燥、　そうでなければドロー
            if ( PA[CON].area_deck->number_cards == 0 ) {
                Operation temp20 = {CON, 20, ope_IDused++, processing.uniqueID, {0} } ;
                push( temp20 );   // 焦燥（1/1のダメージ）を受ける
            }
            else {
                DrawCard (CON) ;
            }
            } break;


            case 13: {   // 選ばれたカードを使用する
            fprintf (fp, "%05d:%06d:%d:選ばれたカードを使用する\n", processing.ope_code, processing.uniqueID, CON );
            Operation temp0 = {CON, 0, ope_IDused++, processing.uniqueID, {0} } ;
            push( temp0 );   // カードの使用でゲームが終わるようにしておく
            } break;


            case 14: {   // 基本動作 前進 を行う
            fprintf (fp, "%05d:%06d:%d:基本動作 前進 を行う\n", processing.ope_code, processing.uniqueID, CON );
            if ( areas.area_distance.placed_SAKURA == 0 )
                break ;
            if ( CalcNowDistance() <= other_status.master_distance )
                break ;
            if ( PA[CON].area_aura->limit_upper <= (PA[CON].area_aura->placed_SAKURA+PA[CON].area_aura->placed_FREEZE) )
                break ;
            
            // 全て回避したら
            areas.area_distance.placed_SAKURA -- ;
            PA[CON].area_aura->placed_SAKURA ++ ;
            } break;


            case 15: {   // 基本動作 後退 を行う
            fprintf (fp, "%05d:%06d:%d:基本動作 後退 を行う\n", processing.ope_code, processing.uniqueID, CON );
            if ( 10 <= areas.area_distance.placed_SAKURA )
                break;
            if ( PA[CON].area_aura->placed_SAKURA < 1 )
                break;

            // 全て回避したら
            areas.area_distance.placed_SAKURA ++ ;
            PA[CON].area_aura->placed_SAKURA -- ;
            } break;


            case 16: {   // 基本動作 纏い を行う
            fprintf (fp, "%05d:%06d:%d:基本動作 纏い を行う\n", processing.ope_code, processing.uniqueID, CON );
            if ( areas.area_shadow.placed_SAKURA < 1 )
                break;
            if ( PA[CON].area_aura->limit_upper <= (PA[CON].area_aura->placed_SAKURA+PA[CON].area_aura->placed_FREEZE) )
                break;

            // 全て回避したら
            areas.area_shadow.placed_SAKURA -- ;
            PA[CON].area_aura->placed_SAKURA ++ ;
            } break;


            case 17: {   // 基本動作 宿し を行う
            fprintf (fp, "%05d:%06d:%d:基本動作 宿し を行う\n", processing.ope_code, processing.uniqueID, CON );
            if ( PA[CON].area_aura->placed_SAKURA < 1 )
                break;

            // 全て回避したら
            PA[CON].area_aura->placed_SAKURA -- ;
            PA[CON].area_flare->placed_SAKURA ++ ;
            } break;


            case 18: {   // 基本動作 離脱 を行う
            fprintf (fp, "%05d:%06d:%d:基本動作 離脱 を行う\n", processing.ope_code, processing.uniqueID, CON );
            if ( areas.area_shadow.placed_SAKURA < 1 )
                break;
            if ( 10 <= areas.area_distance.placed_SAKURA )
                break;
            if ( other_status.master_distance < CalcNowDistance() )
                break;

            // 全て回避したら
            areas.area_shadow.placed_SAKURA -- ;
            areas.area_distance.placed_SAKURA ++ ;
            } break;


            case 19: {   // 選ばれたカードを伏札にする
            fprintf (fp, "%05d:%06d:%d:選ばれたカードを伏札にする\n", processing.ope_code, processing.uniqueID, CON );
            // １．伏せ札最後尾にコピー　２，伏せ札の枚数+1　３．空いた場所を詰める関数へ丸投げ
            PA[CON].area_discard->cards_stack[PA[CON].area_discard->number_cards] = processing.detail.cards_stack[processing.detail.card_position] ;
            PA[CON].area_discard->number_cards ++ ;
            SortCardStack (processing.detail.cards_stack, processing.detail.card_position, processing.detail.sauce_cards_num ) ;
            } break;


            case 20: {   // ダメージの解決を行う
            fprintf (fp, "%05d:%06d:%d:ダメージの解決を行う\n", processing.ope_code, processing.uniqueID, CON );
            // 今は空にしておこう
            } break;


            case 21: {   // リシャッフルを行う
            fprintf (fp, "%05d:%06d:%d:リシャッフルを行う\n", processing.ope_code, processing.uniqueID, CON );
            // 自分用メモ。　これ、一応カードの移動で、盤面変化があるのでオペレーションにした方が良いのかも？

            // まず捨て札のカード全てを山札へ移動する。
            for (int i=0; i < PA[CON].area_played->number_cards ; i++) {
                PA[CON].area_deck->cards_stack[PA[CON].area_deck->number_cards] = PA[CON].area_played->cards_stack[i] ;
                PA[CON].area_deck->number_cards ++ ;
                PA[CON].area_played->cards_stack[i] = object_card_formatdata ;
            }
            PA[CON].area_played->number_cards = 0 ;

            // 伏札も同様にする
            for (int i=0; i < PA[CON].area_discard->number_cards ; i++) {
                PA[CON].area_deck->cards_stack[PA[CON].area_deck->number_cards] = PA[CON].area_discard->cards_stack[i] ;
                PA[CON].area_deck->number_cards ++ ;
                PA[CON].area_discard->cards_stack[i] = object_card_formatdata ;
            }
            PA[CON].area_discard->number_cards = 0 ;

            // 最後に山札を入念にシャッフルする
            ShuffleDeck (CON) ;
            } break;





            default:
            printf ("エラー：処理内容が定義されていないオペレーションを解決しようとしました\n");
            fprintf (fp, "エラー：処理内容が定義されていないオペレーションを解決しようとしました\n") ;
            fprintf (fp, "%05dが未定義です", processing.ope_code );
            printf("強制終了します\n") ;
            goto FATAL_ERROR_END ;
        }   // オペIDにより該当するオペを行う段階がこれで終了








        // 以下、状況起因のチェックリスト。全てのEnchantについて、今回の１ループで状況起因が起きていたらそれを申告する
        // この時、複数の申告が同時にあった場合、どの順でスタックに積むかをターンプレイヤーに尋ねる。
        // ただし、「盤面に変化を与えない、内部処理用の記述」もここに書いているので混同しないように。これらはオペレーションをスタックに積むのではなく直接値を操作するし、複数同時発生も気にしない。
        Operation temp_ope_list[50] = {0} ;
        int temp_ope_num = 0 ;

        // ターンプレイヤーの付与札領域に「精霊連携」があるなら
        if ( IsThereCard(PA[TurnP].area_enchant->cards_stack, 990201009, PA[TurnP].area_enchant->number_cards) ) {
            if ( processing.ope_code==1 )   // ターン開始時処理をしたサイクルなら
                other_status.player_status[TurnP].tokens.seireirenkei ++;
        }

        // // ターンプレイヤーの手札領域に「投射」があるなら
        // if ( IsThereCard(PA[TurnP].area_hand->cards_stack, 990101001, PA[TurnP].area_hand->number_cards) ) {
        //     printf("投射あるよ\n");
        // }

        // ターン終了時処理を解決したサイクルなら、エフェクトトークンズをリセットすること。















    }


    









    // printf ( "%d", areas.area_deck_A.cards_stack[3].charge ) ;
    // printf ( "%d", areas.area_deck_A.cards_stack[4].goddess_data ) ;
    // printf ( "%d", areas.area_deck_A.cards_stack[6].type_main ) ;




    // printf("%s\n", areas.area_hand_A.cards_stack[0].name);
    // printf("%d\n", MakeGhostAttackObject( PA[0].area_hand->cards_stack[0], 0).range_list[10] );
    // printf("%d\n", MakeGhostAttackObject( PA[0].area_hand->cards_stack[0], 0).range_list[11] );
    // printf("%d\n", MakeGhostAttackObject( PA[0].area_hand->cards_stack[0], 0).range_list[12] );
    // printf("%d\n", MakeGhostAttackObject( PA[0].area_hand->cards_stack[0], 0).range_list[13] );
    // printf("%d\n", MakeGhostAttackObject( PA[0].area_hand->cards_stack[0], 0).range_list[14] );
    // printf("%d\n", MakeGhostAttackObject( PA[0].area_hand->cards_stack[0], 0).range_list[15] );
    // printf("%d\n", MakeGhostAttackObject( PA[0].area_hand->cards_stack[0], 0).range_list[16] );
    // printf("%d\n", MakeGhostAttackObject( PA[0].area_hand->cards_stack[0], 0).range_list[17] );
    

    fclose(fp);
    return 0;



    // 異常終了用のgoto着地点ラベル
    FATAL_ERROR_END:
    fclose(fp) ;

    return -1;
}