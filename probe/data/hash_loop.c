unsigned f(unsigned *p, unsigned n){unsigned h=0x811c9dc5u;for(unsigned i=0;i<n;i++){h=(h^p[i])*16777619u; h=(h<<5)|(h>>27);}return h;}
