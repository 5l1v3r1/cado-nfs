# return skewness for 1-norm and corresponding 1-norm
skew := proc(f, x, s0) local d, absf, g, h, i, s;
   d := degree (f);
   g := add(abs(coeff(f,x,i))*S^(i-d/2), i=0..d);
   h := diff(g,S);
   s := fsolve(h, S=s0);
   s, subs(S=s, g)
end:

# takes also into account the linear polynomial x-m
skew2 := proc(f, x, m, s0) local absf, g, i, S;
   absf := add(abs(coeff(f,x,i))*x^i, i=0..degree(f,x));
   g := expand(subs(x=S, absf)/S^(degree(f)/2))*(m/sqrt(S)+sqrt(S));
   g := diff(g,S);
   fsolve(g, S=s0)
end:

supnorm := proc(f, s) local d;
   d := degree(f,x);
   max(seq(abs(coeff(f,x,i)*s^(i-d/2)),i=0..d))
end:

norm1 := proc(f, s) local d;
   d := degree(f,x);
   add(abs(coeff(f,x,i)*s^(i-d/2)),i=0..d)
end:

norm2 := proc(f, s) local d;
   d := degree(f,x);
   sqrt(add(coeff(f,x,i)^2*s^(2*i-d),i=0..d))
end:

Norm2 := proc(f, s) local d, F;
   d := degree(f,x);
   F := y^d*subs(x=x/y, f);
   F := subs(x=s*x, F)/s^(d/2);
   sqrt(int(int(F^2, x=-1..1),y=-1..1))
end:

# mimics code in polyselect.c
special_valuation := proc(f, p)
local disc, d, t, pvaluation_disc, p_divides_lc, e, v, g;
   disc := discrim (f, x);
   d := degree (f);
   pvaluation_disc := 0;
   if disc mod p = 0 then
      pvaluation_disc := 1;
      t := disc / p;
      if t mod p = 0 then pvaluation_disc := 2 fi
   fi;
   p_divides_lc := evalb(coeff(f, x, d) mod p = 0);
   if pvaluation_disc = 0 then
      e := nops(Roots(f) mod p);
      if p_divides_lc then e:=e+1 fi;
      p*e/(p^2-1)
   elif pvaluation_disc = 1 then # and p_divides_lc = false
      # the condition p_divides_lc = false does not seem mandatory
      # but we have to add 1 to e of p divides lc(f)
      e := nops(Roots(f) mod p);
      if p_divides_lc then e:=e+1 fi;
      (p*e-1)/(p^2-1)
   else
      v := val0 (f, p) * p;
      if p_divides_lc then
         g := expand(subs(x=1/(p*x), f)*(p*x)^d);
         v := v + val0 (g, p);
      fi;
      v/(p+1)
   fi
end:

get_alpha := proc(f, B) local s, p, e, q, disc;
   s := 0;
   p := 2;
   disc := discrim(f,x);
   while p <= B do
      e := special_valuation (f, p);
      s := s + (1.0 / (p - 1) - e) * log(p * 1.0);
      p := nextprime(p);
   od;
   s;
end:

# returns the smallest value of alpha for p1 <= p <= p2,
# with a degree-d polynomial
min_alpha := proc(d,p1,p2) local p, s;
   s := 0;
   p := nextprime(p1-1);
   while p <= p2 do
      s := s + evalf((1-min(d,p)*p/(p+1))*log(p)/(p-1));
      p := nextprime(p);
   od;
   s
end:

# p-valuation of integer N
valuation := proc(N, p) local v;
   if N=0 then ERROR("input is zero") fi;
   for v from 0 while N mod p^(v+1) = 0 do od;
   v
end:

# estimate average p-valuation of polynomial f in x, from x0 to x1
est_valuation := proc(f0, p, x0, x1, y0, y1) local s, v, w, f, n;
   f := expand(subs(x=x/y,f0)*y^degree(f0));
   s := 0;
   n := 0;
   for v from x0 to x1 do for w from y0 to y1 do
      if igcd(v,w)=1 then
         s:=s+valuation(subs(x=v,y=w,f),p);
         n:=n+1;
      fi
   od od;
   1.0*s/n
end:

# resultant(P,Q,x), with computations with rationals
res := proc(P,Q,x) local q, R;
   q := lcoeff(Q,x);
   if degree(Q)=0 then q^degree(P,x)
   elif degree(Q)>degree(P) then procname(Q,P,x)
   else R:=rem(P,Q,x); q^(degree(P,x)-degree(R,x))*procname(Q,R,x)
   fi
end:

# fraction-free resultant computation
res2 := proc(P0,Q0,x) local P, Q, q, m, d, s;
   P:=P0;
   Q:=Q0;
   if degree(Q,x)>degree(P,x) then procname(Q,P,x)
   else # deg(P) >= deg(Q)
      m := 1;
      d := 1;
      while degree(Q,x)>0 do
         q := lcoeff(Q,x);
         s := degree(P); # multiply by q^deg(P)
         while degree(P,x)>=degree(Q,x) do
            P:=q*P;
            s := s - degree(Q); # divide by q^deg(Q)
            P:=expand(P-lcoeff(P,x)/lcoeff(Q,x)*x^(degree(P,x)-degree(Q,x))*Q);
         od;
         s := s - degree(P); # divide by q^deg(P mod Q)
         if s>0 then m := m*q^s else d:=d*q^(-s) fi;
         P,Q:=Q,P;
      od;
      m/d*Q^degree(P,x);
   fi;
end:

# the following is from Guillaume Hanrot
# determines average valuation for a prime dividing disc(P)
val := proc(P,p)
   (val0(P, p) * p + val0(expand(subs(x=1/(p*x),P)*(p*x)^degree(P)), p))/(p+1)
end:

val0 := proc(P0,p) local v, r, P, Q, P2;
   v := valuation (icontent(P0), p);
   P := P0/p^v;
   Q := diff(P,x);
   for r in Roots(P) mod p do
      if subs(x=r[1],Q) mod p <> 0 then v := v + 1/(p-1)
      else
         P2 := expand(subs(x=p*x+r[1], P));
         v := v + procname(P2, p)/p
      fi
   od;
   v
end:

##############################################################################

# the following procedure decomposes N in base (m,p)
# cf Lemma 2.1 of Kleinjung's paper "On polynomial selection for the
# general number field sieve"
# Example:
# N:=10941738641570527421809707322040357612003732945449205990913842131476349984288934784717997257891267332497625752899781833797076537244027146743531593354333897
# Lemma21(N, 5, 102406, 1197773395291, 639369899891975567556774664501);
Lemma21 := proc(N, d, ad, p, m) local a, r, i, mu;
   a := table();
   r := N;
   a[d] := ad;
   for i from d-1 by -1 to 0 do
      r := (r - a[i+1]*m^(i+1))/p;
      if not type(r, integer) then ERROR("r is not an integer") fi;
      # find -p*m^i/2 <= mu < p*m^i/2 such that r + mu is divisible by m^i
      # and mu = 0 (mod p), thus mu=lambda*p where lambda = -r/p mod m^i
      mu := (-r/p mod (m^i));
      if mu >= m^i/2 then mu:=mu-m^i fi;
      mu := mu*p;
      a[i] := (r + mu) / m^i;
      if not type(a[i], integer) then ERROR("a[i] is not an integer") fi;
   od;
   add(a[i]*x^i, i=0..d)
end:

basem := proc(N, d, m) local a, r, i, mu;
   a := table();
   r := N;
   Digits:=length(N);
   a[d] := round(N/m^d);
   for i from d-1 by -1 to 0 do
      r := r - a[i+1]*m^(i+1);
      a[i] := round(r/m^i);
   od;
   add(a[i]*x^i, i=0..d)
end:

# compute the sup-norm as in Definition 3.1 of Kleinjung's paper
norme := proc(f) local d, i, j, k, s, ai, ok, n, minn, mins;
   minn := infinity;
   d := degree(f);
   for i from 0 to d do
      ai := abs(coeff(f,x,i));
      if ai=0 then next fi;
      for j from i+1 to d do
         # solve |a_i|*s^(i-d/2) = |a_j|*s^(j-d/2)
         s := evalf(abs(coeff(f,x,j)/ai)^(1/(i-j)));
         n := evalf(ai*s^(i-d/2));
         ok := true;
         for k in {$0..d} minus {i,j} while ok do
            ok := evalb(abs(coeff(f,x,k))*s^(k-d/2) < n);
         od;
         if ok and n<minn then minn:=n; mins:=s fi;
      od
   od;
   mins, minn
end:

sup_norm := proc(f, s) local i;
   seq(evalf(abs(coeff(f,x,i))*s^(i-degree(f)/2)), i=0..degree(f))
end:

# given a positive quadratic form q, and a variable v appearing in q,
# returns the largest lambda such that q - lambda*v^2 is still positive
# Example:
# q:=1/9*a0^2+2/21*a0*a2+1/21*a1^2+2/25*a0*a4+2/25*a1*a3+1/25*a2^2+2/21*a2*a4+1/21*a3^2+1/9*a4^2:
# QFoptimize (q, a3); gives 184/13125
QFoptimize := proc (q, v) local vars, A, i, n, ind, t, l, c, j;
   vars := indets (q);
   if not member (v, vars) then ERROR("q does not contain", v) fi;
   n := nops (vars);
   vars := [op(vars minus {v}), v];
   for i to n do ind[op(i,vars)]:=i od;
   # form the matrix A
   A := matrix(n,n,0);
   if type (q, `+`) then l:=convert(q,list) else l:=[q] fi;
   for t in l do
      # t = c*ai*aj or c*ai^2
      c := icontent (t);
      t := t/c;
      if type (t, name^2) then
         i := ind[op(1,t)];
         A[i,i] := c;
      elif type (t, `*`) and type ([op(t)], [name, name]) then
         i := ind[op(1,t)];
         j := ind[op(2,t)];
         A[i,j] := c/2;
         A[j,i] := c/2;
      else ERROR ("unexpected term:", t)
      fi
   od;
   linalg[det](A)/linalg[det](linalg[submatrix](A, 1..n-1, 1..n-1))
end:
