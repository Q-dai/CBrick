/*
###################################################################################
#
# CBrick
#
# Copyright (c) 2017 Research Institute for Information Technology(RIIT),
#                    Kyushu University.  All rights reserved.
#
####################################################################################
*/

/*
 * @file   CB_SubDomain.cpp
 * @brief  SubDomain class
 */

#include "CB_SubDomain.h"

/*
 * @fn findParameter
 * @brief 指定分割数に対応したパラメータを得る
 * @retval true-success, false-fail
 */
bool SubDomain::findParameter()
{
  FILE* fp=NULL;

  Hostonly_
  {
    if ( !(fp=fopen("div_process.txt", "w")) )
    {
      stamped_printf("\tSorry, can't open 'div_process.txt' file. Write failed.\n");
      return false;
    }
  }


  // 候補パラメータ保持クラス
  cntl_tbl tbl;

  // 評価するパラメータを保持する配列
  if ( !(tbl.score = new score_tbl[numProc]) ) {
    printf("\tFail to allocate memory\n");
    return false;
  }

  Hostonly_ {
    printf("G_size = %5d %5d %5d\n\n", G_size[0], G_size[1], G_size[2]);
    fprintf(fp, "G_size = %5d %5d %5d\n\n", G_size[0], G_size[1], G_size[2]);
  }


  // 候補のパラメータを登録
  // 割り切れない場合には、基準サイズを一つだけ大きくしておく
  int vx = G_size[0]/G_div[0];
  if ( G_size[0] != vx*G_div[0] ) vx +=1;

  int vy = G_size[1]/G_div[1];
  if ( G_size[1] != vy*G_div[1] ) vy +=1;

  int vz = G_size[2]/G_div[2];
  if ( G_size[2] != vz*G_div[2] ) vz +=1;

  tbl.dsz[0] = vx;  // 基準サイズ
  tbl.dsz[1] = vy;
  tbl.dsz[2] = vz;

  // 余りの数だけ基準サイズで、残りは基準サイズ-1
  tbl.mod[0] = G_size[0] % G_div[0];
  tbl.mod[1] = G_size[1] % G_div[1];
  tbl.mod[2] = G_size[2] % G_div[2];

  // printout
  Hostonly_ {
    fprintf(fp, "\nDivision parameter\n");
    fprintf(fp, ": div_x div_y div_z : default size(x,y,z) :   mod(x,y,z)\n");
    fprintf(fp, ": %5d %5d %5d :   %5d %5d %5d : %4d %4d %4d\n",
           G_div[0], G_div[1], G_div[2],
           tbl.dsz[0], tbl.dsz[1], tbl.dsz[2],
           tbl.mod[0], tbl.mod[1], tbl.mod[2]);
    fprintf(fp, "\n\n");
  }


  score_tbl* pp=NULL;
  int pin[3];

#ifdef _DEBUG
  Hostonly_ fprintf(fp, "\tRank :     (s_x, s_y, s_z) :        vol          srf         sxy\n");
#endif // _DEBUG

  int m = 0;
  for (int k=0; k<G_div[2]; k++) {
    for (int j=0; j<G_div[1]; j++) {
      for (int i=0; i<G_div[0]; i++) {
        pin[0] = i;
        pin[1] = j;
        pin[2] = k;

        getSizeCell(&tbl, pin, m);

        pp = &tbl.score[m];

        if ( type == "node") getSizeNode(pp);

        getSrf(pp);

        float vol = (float)pp->sz[0] * (float)pp->sz[1] * (float)pp->sz[2];
        float srf = pp->srf;
        float sxy = pp->sxy;

#ifdef _DEBUG
        Hostonly_ fprintf(fp, "\t%4d :  %5d %5d %5d  : %10.3e : %10.3e  %10.3e\n",
                 m, pp->sz[0], pp->sz[1], pp->sz[2],
                 vol, srf, sxy);
#endif // _DEBUG

        m++;
      }
    }
  }

  Hostonly_ fprintf(fp, "\n");

  // 評価値の計算
  Evaluation(&tbl, 1, fp);



  // 決定した分割パラメータをsd[]に保存
  for (int i=0; i<numProc; i++) {
    score_tbl* p = &tbl.score[i];
    sd[i].sz[0] = p->sz[0];
    sd[i].sz[1] = p->sz[1];
    sd[i].sz[2] = p->sz[2];
  }

  // 最終候補に対して、各サブドメインのヘッドインデクスの計算
  getHeadIndex();

  // SubDomainクラスのメンバ変数にコピー
  size[0] = sd[myRank].sz[0];
  size[1] = sd[myRank].sz[1];
  size[2] = sd[myRank].sz[2];

  head[0] = sd[myRank].hd[0];
  head[1] = sd[myRank].hd[1];
  head[2] = sd[myRank].hd[2];

#ifdef _DEBUG
  Hostonly_ {
    fprintf(fp, "\t    Rank :    sz_X    sz_Y    sz_Z :    hd_X    hd_Y    hd_Z\n");
    for (int k=0; k<G_div[2]; k++) {
      for (int j=0; j<G_div[1]; j++) {
        for (int i=0; i<G_div[0]; i++) {
          int r = rank_idx_0(i, j, k, G_div[0], G_div[1]);
          fprintf(fp, "\t%8d : %7d %7d %7d : %7d %7d %7d\n", r,
                  sd[r].sz[0], sd[r].sz[1], sd[r].sz[2],
                  sd[r].hd[0], sd[r].hd[1], sd[r].hd[2]);
        }
      }
    }
  }
#endif // _DEBUG

  // ワーク配列の後始末
  if ( !tbl.score ) delete [] tbl.score;

  Hostonly_ {
    if ( !fp ) fclose(fp);
  }

  return true;
}


/*
 * @fn findOptimalDivision
 * @brief 最適な分割数を見つける
 * @retval true-success, false-fail
 */
bool SubDomain::findOptimalDivision()
{
  if (div_mode == 1) return findParameter();



  FILE* fp=NULL;

  Hostonly_
  {
    if ( !(fp=fopen("div_process.txt", "w")) )
    {
      stamped_printf("\tSorry, can't open 'div_process.txt' file. Write failed.\n");
      return false;
    }
  }

  // 候補の数を因数分解的に数え上げる
  int tbl_size = getNumCandidates();


  // 候補配列の確保
  cntl_tbl* tbl=NULL;
  if ( !(tbl = new cntl_tbl[tbl_size]) ) {
    printf("\tFail to allocate memory\n");
    return false;
  }


  // 評価するパラメータを保持する配列
  for (int i=0; i<tbl_size; i++)
  {
    if ( !(tbl[i].score = new score_tbl[numProc]) ) {
      printf("\tFail to allocate memory\n");
      return false;
    }
  }

  Hostonly_ {
    printf("\nNumber of division candidates = %d\n\n", tbl_size);
    printf("G_size = %5d %5d %5d\n\n", G_size[0], G_size[1], G_size[2]);

    fprintf(fp, "\nNumber of division candidates = %d\n\n", tbl_size);
    fprintf(fp, "G_size = %5d %5d %5d\n\n", G_size[0], G_size[1], G_size[2]);
  }


  // 候補のパラメータを登録
  registerCandidates(tbl);


  // printout
  Hostonly_ {
    fprintf(fp, "\nCandidates of division\n");
    fprintf(fp, " No : div_x div_y div_z : default size(x,y,z) :   mod(x,y,z)\n");
    for (int i=0; i<tbl_size; i++) {
      fprintf(fp, "%3d : %5d %5d %5d :   %5d %5d %5d : %4d %4d %4d\n",
           i,
           tbl[i].div[0], tbl[i].div[1], tbl[i].div[2],
           tbl[i].dsz[0], tbl[i].dsz[1], tbl[i].dsz[2],
           tbl[i].mod[0], tbl[i].mod[1], tbl[i].mod[2]);
    }
    fprintf(fp, "\n\n");
  }


  score_tbl* pp=NULL;

  for (int c=0; c<tbl_size; c++) {

#ifdef _DEBUG
    Hostonly_ fprintf(fp, "\nCandiate[%d] : div= %d %d %d : default= %d %d %d : mod= %d %d %d\n",
           c,
           tbl[c].div[0], tbl[c].div[1], tbl[c].div[2],
           tbl[c].dsz[0], tbl[c].dsz[1], tbl[c].dsz[2],
           tbl[c].mod[0], tbl[c].mod[1], tbl[c].mod[2]);
#endif // _DEBUG

    int pin[3];

#ifdef _DEBUG
    Hostonly_ fprintf(fp, "\tRank :     (s_x, s_y, s_z) :        vol          srf         sxy\n");
#endif // _DEBUG

    int m = 0;
    for (int k=0; k<tbl[c].div[2]; k++) {
      for (int j=0; j<tbl[c].div[1]; j++) {
        for (int i=0; i<tbl[c].div[0]; i++) {
          pin[0] = i;
          pin[1] = j;
          pin[2] = k;

          getSizeCell(&tbl[c], pin, m);

          pp = &tbl[c].score[m];

          if ( type == "node") getSizeNode(pp);

          getSrf(pp);


          float vol = (float)pp->sz[0] * (float)pp->sz[1] * (float)pp->sz[2];
          float srf = pp->srf;
          float sxy = pp->sxy;

#ifdef _DEBUG
          Hostonly_ fprintf(fp, "\t%4d :  %5d %5d %5d  : %10.3e : %10.3e  %10.3e\n",
                 m, pp->sz[0], pp->sz[1], pp->sz[2],
                 vol, srf, sxy);
#endif // _DEBUG

          m++;
        }
      }
    }

  }

  Hostonly_ fprintf(fp, "\n");

  // 評価値の計算
  Evaluation(tbl, tbl_size, fp);


  // 計算量の評価指標によるランキング
  int c1 = sortVolume(tbl, tbl_size, fp);

  Hostonly_ {
    printf("Number of 1st candidates = %d\n\n", c1);
    fprintf(fp, "Number of 1st candidates = %d\n\n", c1);
  }

  if ( c1 > 1 ) {
    // 通信量によるランキング
    int c2 = sortComm(tbl, c1, fp);

    Hostonly_ {
      printf("Number of 2nd candidates = %d\n\n", c2);
      fprintf(fp, "Number of 2nd candidates = %d\n\n", c2);
    }

    if ( c2 > 1 ) {
      int c3;

      if (ranking_opt == 0) // Cubical shape 優先
      {
        c3 = sortCube(tbl, c2, fp);
      }
      else
      {
        c3 = sortLenX(tbl, c2, fp);
      }

      Hostonly_ {
        printf("Number of 3rd candidates = %d\n\n", c3);
        fprintf(fp, "Number of 3rd candidates = %d\n\n", c3);
      }

      if ( c3 > 1 ) {
        int c4;

        if (ranking_opt == 0) // Cubical shape 優先の場合は、4段目ではLength
        {
          c4 = sortLenX(tbl, c3, fp);
        }
        else
        {
          c4 = sortCube(tbl, c3, fp);
        }

        Hostonly_ {
          printf("Number of 4th candidates = %d\n\n", c4);
          fprintf(fp, "Number of 4th candidates = %d\n\n", c4);
        }
        if ( c4 > 1 ) {
          Hostonly_ {
            printf("More than two candidates. Then, Choose first one.\n\n");
            fprintf(fp, "More than two candidates. Then, Choose first one.\n\n");
          }
        }

      } // c3
    } // c2
  } // c1

  // 最終案を決定
  G_div[0] = tbl[0].div[0];
  G_div[1] = tbl[0].div[1];
  G_div[2] = tbl[0].div[2];

  Hostonly_ {
    printf("========================\n");
    printf("\tGlobal division = %d %d %d : Original index = %d\n\n", G_div[0], G_div[1], G_div[2], tbl[0].org_idx);

    fprintf(fp, "========================\n");
    fprintf(fp, "\tGlobal division = %d %d %d : Original index = %d\n\n", G_div[0], G_div[1], G_div[2], tbl[0].org_idx);
  }



  // 決定した分割パラメータをsd[]に保存
  for (int i=0; i<numProc; i++) {
    score_tbl* p = &tbl[0].score[i];
    sd[i].sz[0] = p->sz[0];
    sd[i].sz[1] = p->sz[1];
    sd[i].sz[2] = p->sz[2];
  }

  // 最終候補に対して、各サブドメインのヘッドインデクスの計算
  getHeadIndex();

  // SubDomainクラスのメンバ変数にコピー
  size[0] = sd[myRank].sz[0];
  size[1] = sd[myRank].sz[1];
  size[2] = sd[myRank].sz[2];

  head[0] = sd[myRank].hd[0];
  head[1] = sd[myRank].hd[1];
  head[2] = sd[myRank].hd[2];

#ifdef _DEBUG
  Hostonly_ {
    fprintf(fp, "\t    Rank :    sz_X    sz_Y    sz_Z :    hd_X    hd_Y    hd_Z\n");
    for (int k=0; k<G_div[2]; k++) {
      for (int j=0; j<G_div[1]; j++) {
        for (int i=0; i<G_div[0]; i++) {
          int r = rank_idx_0(i, j, k, G_div[0], G_div[1]);
          fprintf(fp, "\t%8d : %7d %7d %7d : %7d %7d %7d\n", r,
                  sd[r].sz[0], sd[r].sz[1], sd[r].sz[2],
                  sd[r].hd[0], sd[r].hd[1], sd[r].hd[2]);
        }
      }
    }
  }
#endif // _DEBUG

  // ワーク配列の後始末
  for (int i=0; i<tbl_size; i++) {
    if ( !tbl[i].score ) delete [] tbl[i].score;
  }
  if ( !tbl ) delete [] tbl;

  Hostonly_ {
    if ( !fp ) fclose(fp);
  }

  return true;
}


/*
 * @fn getNumCandidates
 * @brief 分割数の組み合わせ数を数え上げる
 * @retval 候補の数
 * @note FDM, FVM共通
 */
int SubDomain::getNumCandidates()
{
  int odr=0;
  int np = numProc;

  for (int k=1; k<=np; k++) {
    for (int j=1; j<=np/k+1; j++) {
      for (int i=1; i<=np/(j*k)+1; i++) {

        // 分割候補の積がプロセス数、かつ、分割数が全要素数以下であること
        if ( i*j*k == np && i<=G_size[0] && j<=G_size[1] && k<=G_size[2] ) odr++;
      }
    }
  }

  return odr;
}


/*
 * @fn registerCandidates
 * @brief 分割数の候補パラメータを登録する
 * @param [in,out] tbl 候補配列
 * @note FDM, FVM共通
 */
void SubDomain::registerCandidates(cntl_tbl* tbl)
{
  int odr=0;
  int np = numProc;

  for (int k=1; k<=np; k++) {
    for (int j=1; j<=np/k+1; j++) {
      for (int i=1; i<=np/(j*k)+1; i++) {

        if ( i*j*k == np && i<=G_size[0] && j<=G_size[1] && k<=G_size[2] ) {

          // 割り切れない場合には、基準サイズを一つだけ大きくしておく
          int vx = G_size[0]/i;
          if ( G_size[0] != vx*i ) vx +=1;

          int vy = G_size[1]/j;
          if ( G_size[1] != vy*j ) vy +=1;

          int vz = G_size[2]/k;
          if ( G_size[2] != vz*k ) vz +=1;

          tbl[odr].dsz[0] = vx;  // 基準サイズ
          tbl[odr].dsz[1] = vy;
          tbl[odr].dsz[2] = vz;

          // 余りの数だけ基準サイズで、残りは基準サイズ-1
          tbl[odr].mod[0] = G_size[0] % i;
          tbl[odr].mod[1] = G_size[1] % j;
          tbl[odr].mod[2] = G_size[2] % k;

          tbl[odr].div[0]= i;  // Number of divisions for each direction
          tbl[odr].div[1]= j;
          tbl[odr].div[2]= k;

          tbl[odr].org_idx = odr; // 作成時のリストの順番を記録

          odr++;
        }
      }
    }
  }

}


/*
 * @fn getSizeCell
 * @brief サブドメインのセルサイズを計算(セルベース)
 * @param [in,out]  t       候補配列
 * @param [in]      in[3]   候補配列インデクス
 * @param [in]      m       ランク番号
 * @note 配列の先頭 0 から数えてmod[]未満は標準、それ以降は一つ少ない。
 *       ただし、mod[]==0の場合は標準サイズ
 *
 *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14
 * cell   0  1  2  3  0  1  2  3  0  1  2  3  0  1  2
 *      |--+--+--+--|--+--+--+--|--+--+--+--|--+--+--|
 * node 0  1  2  3  4           0  1  2  3  4
                    0  1  2  3  4           0  1  2  3
        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
 */
void SubDomain::getSizeCell(cntl_tbl* t, const int* in, const int m)
{
  int tmp[3];

  if (t->mod[0] == 0) {
    tmp[0] = t->dsz[0];
  }
  else {
    tmp[0] = (in[0] < t->mod[0]) ? t->dsz[0] : t->dsz[0]-1;
  }

  if (t->mod[1] == 0) {
    tmp[1] = t->dsz[1];
  }
  else {
    tmp[1] = (in[1] < t->mod[1]) ? t->dsz[1] : t->dsz[1]-1;
  }

  if (t->mod[2] == 0) {
    tmp[2] = t->dsz[2];
  }
  else {
    tmp[2] = (in[2] < t->mod[2]) ? t->dsz[2] : t->dsz[2]-1;
  }

  t->score[m].sz[0] = tmp[0];
  t->score[m].sz[1] = tmp[1];
  t->score[m].sz[2] = tmp[2];
}



/*
 * @fn getSizeNode
 * @brief サブドメインのセルサイズを計算(ノードベース)
 * @param [in,out]  t       候補配列のスコアクラス
 * @note 配列の先頭 0 から数えてmod[]未満は標準、それ以降は一つ少ない。
 *       ただし、mod[]==0の場合は標準サイズ
 *
 *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14
 * cell   0  1  2  3  0  1  2  3  0  1  2  3  0  1  2
 *      |--+--+--+--|--+--+--+--|--+--+--+--|--+--+--|
 * node 0  1  2  3  4           0  1  2  3  4
                    0  1  2  3  4           0  1  2  3
        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
 */
void SubDomain::getSizeNode(score_tbl* t)
{
  t->sz[0] += 1;
  t->sz[1] += 1;
  t->sz[2] += 1;
}


/*
 * @fn getSrf
 * @brief サブドメインの表面積を計算
 * @param [in,out]  t       候補配列のスコアクラス
 */
void SubDomain::getSrf(score_tbl* t)
{
  float d0 = (float)t->sz[0];
  float d1 = (float)t->sz[1];
  float d2 = (float)t->sz[2];
  float s0 = d1 * d2;
  float s1 = d2 * d0;
  float s2 = d0 * d1;

  // 表面積
  t->srf =( s0 + s1 + s2 ) * 2.0;

  // 立方体への近さを表す自乗量
  // s0 > s1 > s2にならべ、s1を基準に計算する
  if (s1 < s2) {
    float tmp = s1;
    s1 = s2;
    s2 = tmp;
  }
  if (s0 < s1) {
    float tmp = s0;
    s0 = s1;
    s1 = tmp;

    if (s1 < s2) {
      float tmp = s1;
      s1 = s2;
      s2 = tmp;
    }
  }

  t->sxy = (s0-s1)*(s0-s1) + (s2-s1)*(s2-s1);
}



/*
 * @fn Evaluation
 * @brief 評価値を計算
 * @param [in,out]  t       候補配列
 * @param [in]      tbl_sz  配列サイズ
 * @param [in]      fp      file pointer
 */
void SubDomain::Evaluation(cntl_tbl* t, const int tbl_sz, FILE* fp)
{
  Hostonly_ {
    fprintf(fp, "\nVolume index    >>  smaller balance is better. \n");
    fprintf(fp, " No :      Vol_Min      Vol_Max  Balance\n");
  }

  for (int i=0; i<tbl_sz; i++)
  {
    float v_min=FLT_MAX;
    float v_max=FLT_MIN;

    for (int m=0; m<numProc; m++)
    {
      score_tbl* p = &t[i].score[m];
      float vol = p->sz[0] * p->sz[1] * p->sz[2];
      if (vol < v_min) v_min = vol;
      if (vol > v_max) v_max = vol;
    }
    t[i].sc_vol = (v_max-v_min)/v_max;

    Hostonly_ fprintf(fp, "%3d : %12.3e %12.3e %8.3f\n",i, v_min, v_max, t[i].sc_vol);
  }

  Hostonly_ fprintf(fp, "\n");


  Hostonly_ {
    fprintf(fp, "\nCommunication index  >> smaller surface, longer length, and smaller cubical are better.\n");
    fprintf(fp, " No :      surface     length       cubical\n");
  }

  for (int i=0; i<tbl_sz; i++)
  {
    float c_sum= 0.0;
    int lmax = -1;
    float cubic = 0.0;

    for (int m=0; m<numProc; m++)
    {
      score_tbl* p = &t[i].score[m];
      c_sum  += p->srf;
      if ( lmax < p->sz[0] ) lmax = p->sz[0];
      cubic += p->sxy;
    }
    t[i].sc_com = c_sum;
    t[i].sc_len = (float)lmax;
    t[i].sc_hex = cubic;

    Hostonly_ fprintf(fp, "%3d : %12.3e   %8.0f  %12.3e\n",i, t[i].sc_com, t[i].sc_len, t[i].sc_hex);
  }

  Hostonly_ fprintf(fp, "\n");

}


/*
 * @fn sortVolume
 * @brief 計算ボリュームの点でソート
 * @param [in,out]  t       候補配列
 * @param [in]      tbl_sz  配列サイズ
 * @param [in]      fp      file pointer
 * @retval 第一レベルの候補数、必ず 1 以上
 */
int SubDomain::sortVolume(cntl_tbl* t, const int tbl_sz, FILE* fp)
{
  Hostonly_ {
    fprintf(fp, "\n1st screening by volume balance\n");
    printf("\n1st screening by volume balance\n");
  }

  for (int i=0; i<tbl_sz; i++)
  {
    for (int j=tbl_sz-1; j>i; j--)
    {
      if (t[j].sc_vol < t[j-1].sc_vol)
      {
        cntl_tbl p = t[j];
        t[j] = t[j-1];
        t[j-1] = p;
      }
    }
  }


  Hostonly_ {
    // for file
    fprintf(fp, " No :  Balance  org_index\n");
    for (int i=0; i<tbl_sz; i++) {
      fprintf(fp, "%3d : %8.3f %10i\n",i, t[i].sc_vol, t[i].org_idx);
    }
    fprintf(fp, "\n");

    // for stdout
    printf(" No :  Balance  org_index\n");
    int c_sz = (10 < tbl_sz) ? 10 : tbl_sz;
    for (int i=0; i<c_sz; i++) {
      printf("%3d : %8.3f %10i\n",i, t[i].sc_vol, t[i].org_idx);
    }
    printf("\n");
  }

  // 第一レベルの候補を絞り込み
  // 最小値を持つものがいくつあるか
  float v_min = t[0].sc_vol;
  int count = 0;
  for (int i=1; i<tbl_sz; i++)
  {
    if ( v_min == t[i].sc_vol) count++;
  }

  return count+1;
}


/*
 * @fn sortComm
 * @brief 通信量の点でソート
 * @param [in,out]  t      候補配列
 * @param [in]      c_sz   対象候補数
 * @param [in]      fp     file pointer
 * @retval 候補数、必ず 1 以上
 * @note 小さい順にソート
 */
int SubDomain::sortComm(cntl_tbl* t, const int c_sz, FILE* fp)
{
  Hostonly_ {
    fprintf(fp, "\n2nd screening by amount of communication\n");
    printf("\n2nd screening by amount of communication\n");
  }

  for (int i=0; i<c_sz; i++)
  {
    for (int j=c_sz-1; j>i; j--)
    {
      if (t[j].sc_com < t[j-1].sc_com)
      {
        cntl_tbl p = t[j];
        t[j] = t[j-1];
        t[j-1] = p;
      }
    }
  }

  Hostonly_ {
    // for file
    fprintf(fp, " No : Communication  org_index\n");
    for (int i=0; i<c_sz; i++) {
      fprintf(fp, "%3d : %12.3e  %10i\n",i, t[i].sc_com, t[i].org_idx);
    }
    fprintf(fp, "\n");

    // for stdout
    printf(" No : Communication  org_index\n");
    int m_sz = (10 < c_sz) ? 10 : c_sz;
    for (int i=0; i<m_sz; i++) {
      printf("%3d : %12.3e  %10i\n",i, t[i].sc_com, t[i].org_idx);
    }
    printf("\n");
  }

  // 第一レベルの候補を絞り込み
  // 最小値を持つものがいくつあるか
  float c_min = t[0].sc_com;
  int count = 0;
  for (int i=1; i<c_sz; i++)
  {
    if ( c_min == t[i].sc_com) count++;
  }

  return count+1;
}


/*
 * @fn sortLenX
 * @brief X方向の長さ（ベクトル長）の点でソート
 * @param [in,out]  t      候補配列
 * @param [in]      c_sz   対象候補数
 * @param [in]      fp     file pointer
 * @retval 候補数
 * @note 大きい順にソート
 */
int SubDomain::sortLenX(cntl_tbl* t, const int c_sz, FILE* fp)
{
  Hostonly_ {
    fprintf(fp, "\nScreening by Vector length in X\n");
    printf("\nScreening by Vector length in X\n");
  }

  for (int i=0; i<c_sz; i++)
  {
    for (int j=c_sz-1; j>i; j--)
    {
      if (t[j].sc_len > t[j-1].sc_len)
      {
        cntl_tbl p = t[j];
        t[j] = t[j-1];
        t[j-1] = p;
      }
    }
  }

  Hostonly_ {
    // for file
    fprintf(fp, " No :  X-length   org_index\n");
    for (int i=0; i<c_sz; i++) {
      fprintf(fp, "%3d : %12.3e %8i\n",i, t[i].sc_len, t[i].org_idx);
    }
    fprintf(fp, "\n");

    // for stdout
    printf(" No :    X-length   org_index\n");
    int m_sz = (10 < c_sz) ? 10 : c_sz;
    for (int i=0; i<m_sz; i++) {
      printf("%3d : %12.3e %8i\n",i, t[i].sc_len, t[i].org_idx);
    }
    printf("\n");
  }

  // 第一レベルの候補を絞り込み
  // 最小値を持つものがいくつあるか
  float c_min = t[0].sc_len;
  int count = 0;
  for (int i=1; i<c_sz; i++)
  {
    if ( c_min == t[i].sc_len) count++;
  }

  return count+1;
}



/*
 * @fn sortCube
 * @brief 立方体への近さの点でソート
 * @param [in,out]  t      候補配列
 * @param [in]      c_sz   対象候補数
 * @param [in]      fp     file pointer
 * @retval 候補数、必ず 1 以上
 * @note 小さい順にソート
 */
int SubDomain::sortCube(cntl_tbl* t, const int c_sz, FILE* fp)
{
  Hostonly_ {
    fprintf(fp, "\nScreening by cubical shape\n");
    printf("\nScreening by cubical shape\n");
  }

  for (int i=0; i<c_sz; i++)
  {
    for (int j=c_sz-1; j>i; j--)
    {
      if (t[j].sc_hex < t[j-1].sc_hex)
      {
        cntl_tbl p = t[j];
        t[j] = t[j-1];
        t[j-1] = p;
      }
    }
  }

  Hostonly_ {
    // for file
    fprintf(fp, " No :  CubicalShape  org_index\n");
    for (int i=0; i<c_sz; i++) {
      fprintf(fp, "%3d : %12.3e  %10i\n",i, t[i].sc_hex, t[i].org_idx);
    }
    fprintf(fp, "\n");

    // for stdout
    printf(" No :  CubicalShape  org_index\n");
    int m_sz = (10 < c_sz) ? 10 : c_sz;
    for (int i=0; i<m_sz; i++) {
      printf("%3d : %12.3e  %10i\n",i, t[i].sc_hex, t[i].org_idx);
    }
    printf("\n");
  }

  // 第一レベルの候補を絞り込み
  // 最小値を持つものがいくつあるか
  float c_min = t[0].sc_hex;
  int count = 0;
  for (int i=1; i<c_sz; i++)
  {
    if ( c_min == t[i].sc_hex) count++;
  }

  return count+1;
}


/*
 * @fn getHeadIndex
 * @brief 各サブドメインの先頭のグローバルインデクスを計算
 * @note インデクスは、ゼロから開始、cell, nodeとも同じ値になる
 *
 * Rank |     0     |     1     |     2     |    3   |
 *        0           4           8          12        << head
 *        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14  << Global
 * cell   0  1  2  3  0  1  2  3  0  1  2  3  0  1  2  << Local
 *      |--+--+--+--|--+--+--+--|--+--+--+--|--+--+--|
 * node 0  1  2  3  4           0  1  2  3  4          << Local
 *                  0  1  2  3  4           0  1  2  3 << Local
 *      0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 << Global
 *      0           4           8          12          << head
 */
void SubDomain::getHeadIndex()
{
  int i,j,k;
  int nx = G_div[0];
  int ny = G_div[1];
  int nz = G_div[2];

  // i=0の位置のプロセスのHeadIndexを0に初期化する
  i = 0;
  for (k=0; k<nz; k++) {
    for (j=0; j<ny; j++) {
      int r = rank_idx_0(i, j, k, nx, ny);
      sd[r].hd[0] = 0;
    }
  }

  for (k=0; k<nz; k++) {
    for (j=0; j<ny; j++) {
      for (i=1; i<nx; i++) {
        int r0 = rank_idx_0(i,   j, k, nx, ny);
        int r1 = rank_idx_0(i-1, j, k, nx, ny);
        sd[r0].hd[0] = sd[r1].hd[0] + sd[r1].sz[0];
      }
    }
  }

  // j=0の位置のプロセスのHeadIndexを0に初期化する
  j = 0;
  for (k=0; k<nz; k++) {
    for (i=0; i<nx; i++) {
      int r = rank_idx_0(i, j, k, nx, ny);
      sd[r].hd[1] = 0;
    }
  }

  for (k=0; k<nz; k++) {
    for (i=0; i<nx; i++) {
      for (j=1; j<ny; j++) {
        int r0 = rank_idx_0(i, j,   k, nx, ny);
        int r1 = rank_idx_0(i, j-1, k, nx, ny);
        sd[r0].hd[1] = sd[r1].hd[1] + sd[r1].sz[1];
      }
    }
  }

  // k=0の位置のプロセスのHeadIndexを0に初期化する
  k = 0;
  for (j=0; j<ny; j++) {
    for (i=0; i<nx; i++) {
      int r = rank_idx_0(i, j, k, nx, ny);
      sd[r].hd[2] = 0;
    }
  }

  for (j=0; j<ny; j++) {
    for (i=0; i<nx; i++) {
      for (k=1; k<nz; k++) {
        int r0 = rank_idx_0(i, j, k,   nx, ny);
        int r1 = rank_idx_0(i, j, k-1, nx, ny);
        sd[r0].hd[2] = sd[r1].hd[2] + sd[r1].sz[2];
      }
    }
  }

}



/*
* @fn createRankTable
* @brief 通信テーブルを作成
* @param [in,out]  t      候補配列
* @param [in]      c_sz   対象候補数
* @param [in]      fp     file pointer
* @note In case of G_div=4, array size is G_div+2
*
* Rank |   -1   |    0    |    1    |    2    |    3    |    4    |
*        halo   <---------------  Inner region   ------->   halo
*/
bool SubDomain::createRankTable()
{
  int nx = G_div[0];
  int ny = G_div[1];
  int nz = G_div[2];

  // 配列サイズはガイドセル幅(halo)=1
  int* rt = NULL;
  int m_sz = (nx+2) * (ny+2) * (nz+2);

  if ( !(rt = new int[m_sz] )) {
    printf("fail to allocate memory [rank=%d]\n", myRank);
    return false;
  }

  // -1で初期化
#pragma omp parallel for
  for (int i=0; i<m_sz; i++) {
    rt[i] = -1;
  }

  // rank number
  int c = 0;
  #pragma omp parallel for collapse(2)
  for (int k=0; k<nz; k++) {
    for (int j=0; j<ny; j++) {
      for (int i=0; i<nx; i++) {
        rt[_IDX_S3D(i, j, k, nx, ny, 1)] = c++;
      }
    }
  }

  // Neighbor rank ID for comm
  int m = 0;
  #pragma omp parallel for collapse(2)
  for (int k=0; k<nz; k++) {
    for (int j=0; j<ny; j++) {
      for (int i=0; i<nx; i++) {
        sd[m].cm[X_minus] = rt[_IDX_S3D(i-1, j  , k  , nx, ny, 1)];
        sd[m].cm[X_plus]  = rt[_IDX_S3D(i+1, j  , k  , nx, ny, 1)];
        sd[m].cm[Y_minus] = rt[_IDX_S3D(i  , j-1, k  , nx, ny, 1)];
        sd[m].cm[Y_plus]  = rt[_IDX_S3D(i  , j+1, k  , nx, ny, 1)];
        sd[m].cm[Z_minus] = rt[_IDX_S3D(i  , j  , k-1, nx, ny, 1)];
        sd[m].cm[Z_plus]  = rt[_IDX_S3D(i  , j  , k+1, nx, ny, 1)];
        m++;
      }
    }
  }

  if ( !rt ) delete [] rt;
  rt = NULL;


  // rank ID をメンバ変数にコピー
  for (int i=0; i<NOFACE; i++) {
    comm_tbl[i] = sd[myRank].cm[i];
  }


#ifdef _DEBUG
  // 確認のため出力
  Hostonly_ {
    printf("\n==================\n");
    printf("\tGenerate Rank Table\n\n");

    FILE* fp=NULL;
    if ( !(fp=fopen("div_process.txt", "a")) )
    {
      stamped_printf("\tSorry, can't open 'div_process.txt' file. Write failed.\n");
      return false;
    }
    else
    {
      fprintf(fp, "\n==================\n");
      fprintf(fp,"\tGenerate Rank Table\n\n");
      fprintf(fp, "    Rank :  X_minus   X_plus  Y_minus   Y_plus  Z_minus   Z_plus\n");

      for (int i=0; i<numProc; i++)
      {
        SubdomainInfo* s = &sd[i];
        fprintf(fp, "%8d : %8d %8d %8d %8d %8d %8d\n", i,
                s->cm[0], s->cm[1], s->cm[2],
                s->cm[3], s->cm[4], s->cm[5]);
      }

      if ( !fp ) fclose(fp);
    }
  }
#endif // _DEBUG

  // ワーク用 SubDomain クラス配列の破棄
  if ( !sd ) delete [] sd;

  return true;
}