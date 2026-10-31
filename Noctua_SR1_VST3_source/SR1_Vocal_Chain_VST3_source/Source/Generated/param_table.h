// Autogenerado por tools/gen.py - no editar
#pragma once
struct ParamInfo { int wasmId; const char* id; const char* label; int group; enum Kind{Float,Bool,Choice} kind;
  const char* curve; const char* unit; float minV, maxV, defV; const char* choices; /* separados por | */ };
static const char* const kGroupNames[] = {"Input","Tone","Dynamics","Saturation","Space","Output"};
static const int kNumGroups = 6;
static const ParamInfo kParams[] = {
  {0,"InGain","In Gain",0,ParamInfo::Float,"linear","db",-24.0f,12.0f,6.0f,""},
  {1,"PreSat","Pre Sat",0,ParamInfo::Float,"linear","db",0.0f,12.0f,6.0f,""},
  {2,"HPF","HPF",0,ParamInfo::Float,"exponential","hz",20.0f,300.0f,100.0f,""},
  {3,"LowMid","Low Mid",1,ParamInfo::Float,"linear","db",-12.0f,12.0f,-3.0f,""},
  {4,"Body","Body",1,ParamInfo::Float,"linear","db",-12.0f,12.0f,2.0f,""},
  {5,"Presence","Presence",1,ParamInfo::Float,"linear","db",-12.0f,12.0f,4.0f,""},
  {6,"HarshCut","Harsh Cut",1,ParamInfo::Float,"linear","db",-12.0f,0.0f,-0.5f,""},
  {7,"DeEss","De-Ess",2,ParamInfo::Float,"linear","db",-40.0f,0.0f,-12.0f,""},
  {8,"CompThresh","Thresh",2,ParamInfo::Float,"linear","db",-40.0f,0.0f,-20.0f,""},
  {9,"CompRatio","Ratio",2,ParamInfo::Float,"linear","ratio",1.0f,10.0f,6.0f,""},
  {10,"CompAttack","Attack",2,ParamInfo::Float,"squared","ms",1.0f,100.0f,5.0f,""},
  {11,"CompRelease","Release",2,ParamInfo::Float,"squared","ms",10.0f,500.0f,60.0f,""},
  {12,"SatDrive","Drive",3,ParamInfo::Float,"linear","%",0.0f,100.0f,70.0f,""},
  {13,"SatMix","Mix",3,ParamInfo::Float,"linear","%",0.0f,100.0f,55.0f,""},
  {14,"Exciter","Exciter",3,ParamInfo::Float,"linear","db",0.0f,6.0f,3.5f,""},
  {15,"ReverbTime","Reverb Time",4,ParamInfo::Float,"squared","s",0.2f,5.0f,0.6f,""},
  {16,"ReverbMix","Reverb Mix",4,ParamInfo::Float,"linear","%",0.0f,100.0f,5.0f,""},
  {17,"DelayMix","Delay Mix",4,ParamInfo::Float,"linear","%",0.0f,100.0f,3.0f,""},
  {18,"OutCeiling","Ceiling",5,ParamInfo::Float,"linear","db",-6.0f,0.0f,-0.5f,""},
};
static const int kNumParams = 19;
