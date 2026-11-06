#pragma once
enum class DBRequestAction { Forward,Reject,Simulate };
inline DBRequestAction dbRequestAction(bool synthetic,bool activeOwned,bool live,bool validValue){
 if(!synthetic)return DBRequestAction::Forward;
 return activeOwned && live && validValue ? DBRequestAction::Simulate : DBRequestAction::Reject;
}
