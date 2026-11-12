#include "BootstrapPending.hpp"
#include <cassert>
#include <cstdio>
int main(){dbstate::BootstrapPending<int,2> p;int a=1,b=2,c=3,fa=4,fb=5,fc=6;
 assert(p.offer(&a,&fa)==dbstate::Offer::Inserted);
 assert(p.offer(&a,&fa,true)==dbstate::Offer::Existing&&p.recoveryPending(&a,&fa));
 assert(p.offer(&b,&fb)==dbstate::Offer::Inserted);
 assert(p.offer(&c,&fc)==dbstate::Offer::Full);
 auto t=p.take(0);assert(t.display==&a&&t.frame==&fa&&t.recovery);
 assert(!p.take(0).display);
 assert(p.offer(&c,&fc)==dbstate::Offer::Inserted);
 auto u=p.take(1);assert(u.display==&b&&u.frame==&fb);
 assert(!p.take(2).display);
 puts("bootstrap admission: recovery dedupe, bounded capacity, no overwrite, ownership transfer and reuse pass");}
