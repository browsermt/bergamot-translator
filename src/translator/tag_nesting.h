#ifndef BERGAMOT_TRANSLATOR_TAG_NESTING_H_
#define BERGAMOT_TRANSLATOR_TAG_NESTING_H_

#define MAXSRCLEN 128
#define MAXTGTLEN 256

#define TAGTREEBUFSZ 1024
#define TAGSTRBUFSZ 4096
#define CHILDBUFSZ 2048

double inside[MAXSRCLEN][MAXSRCLEN][MAXTGTLEN];
double outside[MAXSRCLEN][MAXSRCLEN][MAXTGTLEN];

typedef struct Interval
{
  size_t left;
  size_t right;
} Interval;

typedef struct TagNode{
  size_t parent;
  Interval bound;
  Interval label;
  Interval child;
} TagNode;

size_t tagtreesize;
TagNode tagtree[TAGTREEBUFSZ];
char tagstrbuf[TAGSTRBUFSZ];
size_t childbuf[CHILDBUFSZ];

double alignProbability(marian::data::SoftAlignment &align, size_t s, size_t t){
  return align[t][s];
}

void fillInsideNaive(marian::data::SoftAlignment &align, size_t srclen, size_t tgtlen){
  for (size_t t = 0; t < tgtlen; t++)
  {
    for (size_t i = 0; i < srclen; i++)
    {
      inside[i][i][t] = alignProbability(align, i, t);
      for (size_t j = i + 1; j < srclen; j++)
      {
        inside[i][j][t] = inside[i][j-1][t] + alignProbability(align, j, t);
        outside[i][j][t] = 1.0 - inside[i][j-1][t];
      }
    }
  }
}

Interval maxProduct(double *vec, Interval q){
  double max = 0;
  Interval maxi;

  for (size_t l = q.left; l < q.right; l++)
  {
    double product = 1;
    for (size_t r = l; r < q.right; r++)
    {
      product *= vec[r];
      if (max < product)
      {
        max = product;
        maxi.left = l;
        maxi.right = r+1;
      }
    }
  }

  printf("max: %.2lf\n", max);
  return maxi;
}

Interval traverseAndQuery(size_t idx, Interval self_outer){
  Interval child_outer = self_outer;
  Interval self_inner;

  self_inner.left = self_outer.right;
  self_inner.right = self_outer.left;

  for (size_t child_idx = tagtree[idx].child.left; child_idx < tagtree[idx].child.right; child_idx++)
  {
    Interval child_range = traverseAndQuery(child_idx, child_outer);
    child_outer.left = child_range.right;

    self_inner.left = (self_inner.left > child_range.left) ? child_range.left : self_inner.left;
    self_inner.right = (self_inner.right < child_range.right) ? child_range.right : self_inner.left;
  }
  double *vec = inside[tagtree[idx].bound.left][tagtree[idx].bound.right];
  return maxProduct(vec, self_inner);
}

#endif //BERGAMOT_TRANSLATOR_TAG_NESTING_H_
