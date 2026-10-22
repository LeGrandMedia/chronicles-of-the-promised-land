#include "RTSTypes.h"
#include "RTSNames.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FRTSCatalog& FRTSCatalog::Get() { static FRTSCatalog C; if(C.Defs.Num()==0) C.Load(); return C; }
static FString Str(const TSharedPtr<FJsonObject>& O,const TCHAR* K,const FString& Default=TEXT("")) { FString S; return O->TryGetStringField(K,S)?S:Default; }
static float Num(const TSharedPtr<FJsonObject>& O,const TCHAR* K,float Default) { double N; return O->TryGetNumberField(K,N)?float(N):Default; }
static TArray<FString> Strings(const TSharedPtr<FJsonObject>& O,const TCHAR* K) { TArray<FString> A; const TArray<TSharedPtr<FJsonValue>>* V; if(O->TryGetArrayField(K,V)) for(auto& X:*V) A.Add(X->AsString()); return A; }
void FRTSCatalog::Load() {
 Defs.Empty(); Factions.Empty(); FString Text; TSharedPtr<FJsonObject> Root;
 FFileHelper::LoadFileToString(Text,*(FPaths::ProjectContentDir()/TEXT("Data/catalog.json")));
 if(FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Root) && Root.IsValid()) {
  for(const TCHAR* Group:{TEXT("units"),TEXT("buildings"),TEXT("upgrades")}) {
   const TArray<TSharedPtr<FJsonValue>>* Items; if(!Root->TryGetArrayField(Group,Items)) continue;
   for(auto& Item:*Items) { auto O=Item->AsObject(); if(!O) continue; FRTSDef D;
    D.Id=Str(O,TEXT("id")); D.Name=Str(O,TEXT("name")); D.Kind=Str(O,TEXT("kind")); D.Producer=Str(O,TEXT("producer")); D.Description=Str(O,TEXT("description")); D.Ability=Str(O,TEXT("ability"));D.Icon=Str(O,TEXT("icon"));D.Hotkey=Str(O,TEXT("hotkey"));D.HeroChoice=Str(O,TEXT("age_hero"));D.Perk=Str(O,TEXT("hero_perk"));
    const TSharedPtr<FJsonObject>* Voices=nullptr;if(O->TryGetObjectField(TEXT("voiceLines"),Voices))D.SelectLine=Str(*Voices,TEXT("select"));
    const TSharedPtr<FJsonObject>* Audio=nullptr;if(O->TryGetObjectField(TEXT("voice_audio"),Audio))D.SelectAudio=Str(*Audio,TEXT("select"));
    const TSharedPtr<FJsonObject>* Descs=nullptr;if(O->TryGetObjectField(TEXT("faction_descriptions"),Descs))for(const auto& Pair:(*Descs)->Values){FString Desc;if(Pair.Value->TryGetString(Desc))D.FactionDescriptions.Add(Pair.Key,Desc);}
    const TSharedPtr<FJsonObject>* Ages=nullptr;if(O->TryGetObjectField(TEXT("faction_ages"),Ages))for(const auto& Pair:(*Ages)->Values){double Age;if(Pair.Value->TryGetNumber(Age))D.FactionAges.Add(Pair.Key,FMath::Clamp(int32(Age),1,4));}
    const TSharedPtr<FJsonObject>* Bonuses=nullptr;if(O->TryGetObjectField(TEXT("economy_bonuses"),Bonuses))for(const auto& Pair:(*Bonuses)->Values){double Bonus;if(Pair.Value->TryGetNumber(Bonus))D.EconomyBonuses.Add(Pair.Key,FMath::Clamp(float(Bonus),0.f,5.f));}
    RTSNames::RegisterOriginal(TEXT("definition.")+D.Id,D.Name);
    D.Name=RTSNames::Get(TEXT("definition.")+D.Id,D.Name);
    D.HP=Num(O,TEXT("hp"),100); D.Attack=Num(O,TEXT("attack"),0); D.Interval=Num(O,TEXT("interval"),1.5f); D.Range=Num(O,TEXT("range"),1); D.Speed=Num(O,TEXT("speed"),0); D.Sight=Num(O,TEXT("sight"),10); D.Armor=Num(O,TEXT("armor"),0); D.Pop=Num(O,TEXT("pop"),0); D.Age=Num(O,TEXT("age"),1); D.Seconds=Num(O,TEXT("seconds"),1); D.Requires=Strings(O,TEXT("requires"));
    D.bBuilding=FString(Group)==TEXT("buildings"); D.bUpgrade=FString(Group)==TEXT("upgrades"); D.bHero=D.Kind==TEXT("hero"); D.Radius=Num(O,TEXT("radius"),D.bBuilding?2.f:.45f);
    if(D.Kind==TEXT("beast") || D.Kind==TEXT("ship")) D.Radius=1.05f;
    const TArray<TSharedPtr<FJsonValue>>* Cost; if(O->TryGetArrayField(TEXT("cost"),Cost) && Cost->Num()==3) D.Cost=FVector((*Cost)[0]->AsNumber(),(*Cost)[1]->AsNumber(),(*Cost)[2]->AsNumber());
    D.CementCost=FMath::Max(0.f,Num(O,TEXT("cement_cost"),0));Defs.Add(D);
   }
  }
  const TArray<TSharedPtr<FJsonValue>>* Fs; if(Root->TryGetArrayField(TEXT("factions"),Fs)) for(auto& V:*Fs) { auto O=V->AsObject(); FRTSFaction F; F.Id=Str(O,TEXT("id"));const FString OriginalName=Str(O,TEXT("name"));RTSNames::RegisterOriginal(TEXT("faction.")+F.Id,OriginalName);F.Name=RTSNames::Get(TEXT("faction.")+F.Id,OriginalName);F.Description=Str(O,TEXT("description"));F.Units=Strings(O,TEXT("units"));F.Buildings=Strings(O,TEXT("buildings"));F.Upgrades=Strings(O,TEXT("upgrades"));Factions.Add(F); }
 }
 for(const FString& Id:{TEXT("tree"),TEXT("ore"),TEXT("berries"),TEXT("crate"),TEXT("wild_game")}) { FRTSDef D; D.Id=Id;D.Kind=TEXT("resource"); D.Name=Id==TEXT("wild_game")?TEXT("Wild Game - Food"):Id==TEXT("tree")?TEXT("Forest • Timber"):Id==TEXT("ore")?TEXT("Ore deposit • Precious Things"):Id==TEXT("berries")?TEXT("Wild provisions • Food"):TEXT("Dropped cargo");RTSNames::RegisterOriginal(TEXT("resource.")+Id,Id==TEXT("ore")?TEXT("Ore deposit • Wealth"):D.Name);D.Name=RTSNames::Get(TEXT("resource.")+Id,D.Name); D.Radius=Id==TEXT("ore")?1.4f:.8f;D.HP=Id==TEXT("wild_game")?250:Id==TEXT("ore")?3000:Id==TEXT("tree")?500:1200; D.Sight=0; Defs.Add(D); }
 for(int A=2;A<=3;A++) { FRTSDef D;D.Id=FString::Printf(TEXT("age%d"),A);const FString OriginalName=A==2?TEXT("Fortification Age"):TEXT("Mobilization Age");RTSNames::RegisterOriginal(TEXT("definition.")+D.Id,OriginalName);D.Name=RTSNames::Get(TEXT("definition.")+D.Id,OriginalName);D.bUpgrade=true;D.Producer=TEXT("seat_of_the_people");D.Age=A-1;D.Cost=A==2?FVector(400,0,200):FVector(700,0,400);D.Seconds=A==2?90:120;Defs.Add(D); }
 for(auto& Choice:Defs)if(Choice.Kind==TEXT("age_option"))Choice.Pop=Def(Choice.HeroChoice).Pop;
 UE_LOG(LogTemp,Display,TEXT("RTS catalog: %d definitions, %d factions"),Defs.Num(),Factions.Num());
}
int32 FRTSCatalog::Index(const FString& Id) const { return Defs.IndexOfByPredicate([&](const FRTSDef& D){return D.Id==Id;}); }
const FRTSDef& FRTSCatalog::Def(int32 I) const { static FRTSDef Empty; return Defs.IsValidIndex(I)?Defs[I]:Empty; }
const FRTSDef& FRTSCatalog::Def(const FString& Id) const { return Def(Index(Id)); }
const FRTSFaction& FRTSCatalog::Faction(const FString& Id) const { static FRTSFaction Empty; for(auto& F:Factions) if(F.Id==Id) return F;return Empty; }
FVector FRTSCatalog::Price(const FRTSDef& D,const FString& F) const { float M=(!D.bBuilding&&!D.bUpgrade&&D.Id!=TEXT("curelom")&&D.Id!=TEXT("elephant"))?(F==TEXT("nephites")?1.25f:F==TEXT("lamanites")?.65f:1.f):1.f;return FVector(FMath::CeilToInt(D.Cost.X*M),FMath::CeilToInt(D.Cost.Y*M),FMath::CeilToInt(D.Cost.Z*M)); }
float FRTSCatalog::Duration(const FRTSDef& D,const FString& F) const { float M=(!D.bBuilding&&!D.bUpgrade&&D.Id!=TEXT("curelom")&&D.Id!=TEXT("elephant"))?(F==TEXT("nephites")?1.2f:F==TEXT("lamanites")?.6f:1.f):1.f;return FMath::CeilToFloat(D.Seconds*M); }
bool RTS::WaterAt(FVector2D P,int32 Map) { return Map==2 && P.X>72 && P.X<88 && !(P.Y>34&&P.Y<46) && !(P.Y>114&&P.Y<126); }
bool RTS::RockAt(FVector2D P,int32 Map) { if(Map==0) return FVector2D::Distance(P,FVector2D(80,55))<5 || FVector2D::Distance(P,FVector2D(80,105))<5; if(Map==1) return FVector2D::Distance(P,FVector2D(80,80))<8 || FVector2D::Distance(P,FVector2D(48,80))<4 || FVector2D::Distance(P,FVector2D(112,80))<4; return false; }
FString RTS::NormalizeChat(const FString& Text) { FString Out; for(TCHAR C:Text) { if(C>=0xFF01&&C<=0xFF5E) C-=0xFEE0; if(C==0x3000) C=' '; Out.AppendChar(FChar::ToLower(C)); } return Out; }
bool RTS::HasProfanity(const FString& Text,bool Submitted) {
 static TArray<FString> Words; if(Words.Num()==0) { FString File; FFileHelper::LoadFileToString(File,*(FPaths::ProjectContentDir()/TEXT("Data/profanity.txt"))); File.ParseIntoArrayLines(Words,true); }
 FString Token, N=NormalizeChat(Text); for(int I=0;I<=N.Len();I++) { bool End=I==N.Len(); TCHAR C=End?' ':N[I]; if(!End&&FChar::IsAlpha(C)) Token.AppendChar(C); else { if((!End||Submitted)&&Words.Contains(Token)) return true; Token.Empty(); } } return false;
}
bool RTS::IsApology(const FString& Text) { FString S=NormalizeChat(Text).TrimStartAndEnd(); while(!S.IsEmpty()&&!FChar::IsAlnum(S[0])) S.RightChopInline(1); while(!S.IsEmpty()&&!FChar::IsAlnum(S[S.Len()-1])) S.LeftChopInline(1);return S==TEXT("sorry"); }

FString FRTSCatalog::DisplayName(const FRTSDef& D,const FString& Faction) const {
 return RTSNames::Get(TEXT("faction.")+Faction+TEXT(".definition.")+D.Id,D.Name);
}
FString FRTSCatalog::Description(const FRTSDef& D,const FString& Faction) const {
 const FString* Specific=D.FactionDescriptions.Find(Faction);
 return RTSNames::ResolveText(Specific?*Specific:D.Description);
}
