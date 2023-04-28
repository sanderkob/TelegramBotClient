#pragma once
#ifndef TBDebug_h
#define TBDebug_h

// #define TBCVERBOSE

#ifdef TBCVERBOSE
 #define DOUT(x)        { Serial.print ((x)); \
                          Serial.print ((" -- ")); \
                          Serial.print (__FILE__); Serial.print ((" in line ")); \
                          Serial.print (__LINE__);  Serial.print ((" -- ")); \
                          Serial.println (__FUNCTION__); \
                        }
 #define DOUTKV(k, v)   { Serial.print ((k)); Serial.print (':'); Serial.print (v); \
                          Serial.print ((" -- ")); \
                          Serial.print (__FILE__); Serial.print ((" in line ")); \
                          Serial.print (__LINE__);  Serial.print ((" -- ")); \
                          Serial.println (__FUNCTION__); \
                        }
#else
 #define DOUT(x)        
 #define DOUTKV(k, v)   
#endif

#endif
