#include "Queue.h"
#include "Search.h"
#include <cassert>
int main(){Song a;a.id=1;a.title="Alpha";Song b;b.id=2;b.title="Beta";Song c;c.id=3;c.title="Gamma";Queue q;q.add(a);q.add(b);q.addNext(c);assert(q.items().size()==3);q.setIndex(0);assert(q.next()->id==3);assert(q.previous()->id==1);q.move(2,1);assert(q.items()[1].id==2);q.remove(1);assert(q.items().size()==2);auto r=Search::rank(q.items(),"Alpha");assert(!r.empty()&&r.front().id==1);return 0;}
