/******************************************************************************
Suffix Tree Version 2.1

AUTHORS

Dotan Tsadok
Instructor: Mr. Shlomo Yona, University of Haifa, Israel. December 2002.
Current maintainer: Shlomo Yona	<shlomo@cs.haifa.ac.il>

COPYRIGHT

Copyright 2002-2003 Shlomo Yona

LICENSE

This library is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.


DESCRIPTION OF THIS FILE:

This is the implementation file suffix_tree.c implementing the header file
suffix_tree.h.

This code is an Open Source implementation of Ukkonen's algorithm for
constructing a suffix tree over a string in time and space complexity
O(length of the string). The code is written under strict ANSI C.

For a complete understanding of the code see Ukkonen's algorithm and the
readme.txt file.

The general pseudo code is:

n = length of the string.
ST_CreateTree:
   Calls n times to SPA (Single Phase Algorithm). SPA:  
      Increase the variable e (virtual end of all leaves).
   Calls SEA (Single Extension Algorithm) starting with the first extension that
   does not already exist in the tree and ending at the first extension that
   already exists. SEA :  
      Follow suffix link.
      Check if current suffix exists in the tree.
      If it does not - apply rule 2 and then create a new suffix link.
      apply_rule_2:  
         Create a new leaf and maybe a new internal node as well.
         create_node:  
            Create a new node or a leaf.


For implementation interpretations see Basic Ideas paragraph in the Developement
section of the readme.txt file.

An example of the implementations of a node and its sons using linked lists
instead of arrays:

   (1)
    |
    |
    |
   (2)--(3)--(4)--(5)

(2) is the only son of (1) (call it the first son). Other sons of (1) are
connected using a linked lists starting from (2) and going to the right. (3) is
the right sibling of (2) (and (2) is the left sibling of (3)), (4) is the right
sibling of (3), etc.
The father field of all (2), (3), (4) and (5) points to (1), but the son field
of (1) points only to (2).

*******************************************************************************/

#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "suffix_tree.h"

DBL_WORD    ST_ERROR;

/* See function body */
void ST_PrintTree(SUFFIX_TREE* tree);
/* See function body */
void ST_PrintFullNode(SUFFIX_TREE* tree, NODE* node);

/* Used in function trace_string for skipping (Ukkonen's Skip Trick). */
typedef enum SKIP_TYPE     {skip, no_skip}                 SKIP_TYPE;
/* Used in function apply_rule_2 - two types of rule 2 - see function for more
   details.*/
typedef enum RULE_2_TYPE   {new_son, split}                RULE_2_TYPE;
/* Signals whether last matching position is the last one of the current edge */
typedef enum LAST_POS_TYPE {last_char_in_edge, other_char} LAST_POS_TYPE;

/* Used for statistic measures of speed. */
DBL_WORD counter;
/* Used for statistic measures of space. */
DBL_WORD heap;
/* Used to mark the node that has no suffix link yet. By Ukkonen, it will have
   one by the end of the current phase. */
NODE*    suffixless;

typedef struct SUFFIXTREEPATH
{
   DBL_WORD   begin;
   DBL_WORD   end;
} PATH;

typedef struct SUFFIXTREEPOS
{
   NODE*      node;
   DBL_WORD   edge_pos;
}POS;

/******************************************************************************/
/*
   Define STATISTICS in order to view measures of speed and space while
   constructing and searching the suffix tree. Measures will be printed on the
   screen.
*/
/* #define STATISTICS */

/*
   Define DEBUG in order to view debug printouts to the screen while
   constructing and searching the suffix tree.
*/
/* #define DEBUG */

/******************************************************************************/
/*
   create_node :
   Creates a node with the given init field-values.

  Input : The father of the node, the starting and ending indices 
  of the incloming edge to that node, 
        the path starting position of the node.

  Output: A pointer to that node.
*/


NODE* create_node(NODE* father, DBL_WORD start, DBL_WORD end, DBL_WORD position)
{
   /*Allocate a node.*/
   NODE* node   = (NODE*)malloc(sizeof(NODE));
   if(node == 0)
   {
      printf("\nOut of memory.\n");
      exit(0);
   }

#ifdef STATISTICS
   heap+=sizeof(NODE);
#endif

   /* Initialize node fields. For detailed description of the fields see
      suffix_tree.h */
   node->sons             = 0;
   node->right_sibling    = 0;
   node->left_sibling     = 0;
   node->suffix_link      = 0;
   node->father           = father;
   node->path_position    = position;
   node->edge_label_start = start;
   node->edge_label_end   = end;
   return node;
}

/******************************************************************************/
/*
   find_son :
   Finds son of node that starts with a certain character. 

   Input : the tree, the node to start searching from and the character to be
           searched in the sons.
  
   Output: A pointer to the found son, 0 if no such son.
*/

NODE* find_son(SUFFIX_TREE* tree, NODE* node, unsigned char character)
{
   /* Point to the first son. */
   node = node->sons;
   /* scan all sons (all right siblings of the first son) for their first
   character (it has to match the character given as input to this function. */
   while(node != 0 && tree->tree_string[node->edge_label_start] != character)
   {
#ifdef STATISTICS
      counter++;
#endif
      node = node->right_sibling;
   }
   return node;
}

/******************************************************************************/
/*
   get_node_label_end :
   Returns the end index of the incoming edge to that node. This function is
   needed because for leaves the end index is not relevant, instead we must look
   at the variable "e" (the global virtual end of all leaves). Never refer
   directly to a leaf's end-index.

   Input : the tree, the node its end index we need.

   Output: The end index of that node (meaning the end index of the node's
   incoming edge).
*/

DBL_WORD get_node_label_end(SUFFIX_TREE* tree, NODE* node)
{
   /* If it's a leaf - return e */
   if(node->sons == 0)
      return tree->e;
   /* If it's not a leaf - return its real end */
   return node->edge_label_end;
}

/******************************************************************************/
/*
   get_node_label_length :
   Returns the length of the incoming edge to that node. Uses get_node_label_end
   (see above).

   Input : The tree and the node its length we need.

   Output: the length of that node.
*/

DBL_WORD get_node_label_length(SUFFIX_TREE* tree, NODE* node)
{
   /* Calculate and return the lentgh of the node */
   return get_node_label_end(tree, node) - node->edge_label_start + 1;
}

/******************************************************************************/
/*
   is_last_char_in_edge :
   Returns 1 if edge_pos is the last position in node's incoming edge.

   Input : The tree, the node to be checked and the position in its incoming
           edge.

   Output: the length of that node.
*/

char is_last_char_in_edge(SUFFIX_TREE* tree, NODE* node, DBL_WORD edge_pos)
{
   if(edge_pos == get_node_label_length(tree,node)-1)
      return 1;
   return 0;
}

/******************************************************************************/
/*
   connect_siblings :
   Connect right_sib as the right sibling of left_sib and vice versa.

   Input : The two nodes to be connected.

   Output: None.
*/

void connect_siblings(NODE* left_sib, NODE* right_sib)
{
   /* Connect the right node as the right sibling of the left node */
   if(left_sib != 0)
      left_sib->right_sibling = right_sib;
   /* Connect the left node as the left sibling of the right node */
   if(right_sib != 0)
      right_sib->left_sibling = left_sib;
}

/******************************************************************************/
/*
   apply_extension_rule_2 :
   Apply "extension rule 2" in 2 cases:
   1. A new son (leaf 4) is added to a node that already has sons:
                (1)	       (1)
               /   \	 ->   / | \
              (2)  (3)      (2)(3)(4)

   2. An edge is split and a new leaf (2) and an internal node (3) are added:
              | 	  |
              | 	 (3)
              |     ->   / \
             (1)       (1) (2)

   Input : See parameters.

   Output: A pointer to the newly created leaf (new_son case) or internal node
   (split case).
*/

NODE* apply_extension_rule_2(
                      /* Node 1 (see drawings) */
                      NODE*           node,            
                      /* Start index of node 2's incoming edge */
                      DBL_WORD        edge_label_begin,   
                      /* End index of node 2's incoming edge */
                      DBL_WORD        edge_label_end,      
                      /* Path start index of node 2 */
                      DBL_WORD        path_pos,         
                      /* Position in node 1's incoming edge where split is to be
		         performed */
                      DBL_WORD        edge_pos,         
                      /* Can be 'new_son' or 'split' */
                      RULE_2_TYPE     type)            
{
   NODE *new_leaf,
        *new_internal,
        *son;
   /*-------new_son-------*/
   if(type == new_son)                                       
   {
#ifdef DEBUG   
      printf("rule 2: new leaf (%lu,%lu)\n",edge_label_begin,edge_label_end);
#endif
      /* Create a new leaf (4) with the characters of the extension */
      new_leaf = create_node(node, edge_label_begin , edge_label_end, path_pos);
      /* Connect new_leaf (4) as the new son of node (1) */
      son = node->sons;
      while(son->right_sibling != 0)
         son = son->right_sibling;
      connect_siblings(son, new_leaf);
      /* return (4) */
      return new_leaf;
   }
   /*-------split-------*/
#ifdef DEBUG   
   printf("rule 2: split (%lu,%lu)\n",edge_label_begin,edge_label_end);
#endif
   /* Create a new internal node (3) at the split point */
   new_internal = create_node(
                      node->father,
                      node->edge_label_start,
                      node->edge_label_start+edge_pos,
                      node->path_position);
   /* Update the node (1) incoming edge starting index (it now starts where node
   (3) incoming edge ends) */
   node->edge_label_start += edge_pos+1;

   /* Create a new leaf (2) with the characters of the extension */
   new_leaf = create_node(
                      new_internal,
                      edge_label_begin,
                      edge_label_end,
                      path_pos);
   
   /* Connect new_internal (3) where node (1) was */
   /* Connect (3) with (1)'s left sibling */
   connect_siblings(node->left_sibling, new_internal);   
   /* connect (3) with (1)'s right sibling */
   connect_siblings(new_internal, node->right_sibling);
   node->left_sibling = 0;

   /* Connect (3) with (1)'s father */
   if(new_internal->father->sons == node)            
      new_internal->father->sons = new_internal;
   
   /* Connect new_leaf (2) and node (1) as sons of new_internal (3) */
   new_internal->sons = node;
   node->father = new_internal;
   connect_siblings(node, new_leaf);
   /* return (3) */
   return new_internal;
}

/******************************************************************************/
/*
   trace_single_edge :
   Traces for a string in a given node's OUTcoming edge. It searches only in the
   given edge and not other ones. Search stops when either whole string was
   found in the given edge, a part of the string was found but the edge ended
   (and the next edge must be searched too - performed by function trace_string)
   or one non-matching character was found.

   Input : The string to be searched, given in indices of the main string.

   Output: (by value) the node where tracing has stopped.
           (by reference) the edge position where last match occured, the string
	   position where last match occured, number of characters found, a flag
	   for signaling whether search is done, and a flag to signal whether
	   search stopped at a last character of an edge.
*/

NODE* trace_single_edge(
                      SUFFIX_TREE*    tree, 
                      /* Node to start from */
                      NODE*           node,         
                      /* String to trace */
                      PATH            str,         
                      /* Last matching position in edge */
                      DBL_WORD*       edge_pos,      
                      /* Last matching position in tree source string */
                      DBL_WORD*       chars_found,   
                      /* Skip or no_skip*/
                      SKIP_TYPE       type,          
                      /* 1 if search is done, 0 if not */
                      int*            search_done)   
{
   NODE*      cont_node;
   DBL_WORD   length,str_len;

   /* Set default return values */
   *search_done = 1;
   *edge_pos    = 0;

   /* Search for the first character of the string in the outcoming edge of
      node */
   cont_node = find_son(tree, node, tree->tree_string[str.begin]);
   if(cont_node == 0)
   {
      /* Search is done, string not found */
      *edge_pos = get_node_label_length(tree,node)-1;
      *chars_found = 0;
      return node;
   }
   
   /* Found first character - prepare for continuing the search */
   node    = cont_node;
   length  = get_node_label_length(tree,node);
   str_len = str.end - str.begin + 1;

   /* Compare edge length and string length. */
   /* If edge is shorter then the string being searched and skipping is
      enabled - skip edge */
   if(type == skip)
   {
      if(length <= str_len)
      {
         (*chars_found)   = length;
         (*edge_pos)      = length-1;
         if(length < str_len)
            *search_done  = 0;
      }
      else
      {
         (*chars_found)   = str_len;
         (*edge_pos)      = str_len-1;
      }

#ifdef STATISTICS
      counter++;
#endif

      return node;
   }
   else
   {
      /* Find minimum out of edge length and string length, and scan it */
      if(str_len < length)
         length = str_len;

      for(*edge_pos=1, *chars_found=1; *edge_pos<length; (*chars_found)++,(*edge_pos)++)
      {

#ifdef STATISTICS
         counter++;
#endif

         /* Compare current characters of the string and the edge. If equal - 
	    continue */
         if(tree->tree_string[node->edge_label_start+*edge_pos] != tree->tree_string[str.begin+*edge_pos])
         {
            (*edge_pos)--;
            return node;
         }
      }
   }

   /* The loop has advanced *edge_pos one too much */
   (*edge_pos)--;

   if((*chars_found) < str_len)
      /* Search is not done yet */
      *search_done = 0;

   return node;
}

/******************************************************************************/
/*
   trace_string :
   Traces for a string in the tree. This function is used in construction
   process only, and not for after-construction search of substrings. It is
   tailored to enable skipping (when we know a suffix is in the tree (when
   following a suffix link) we can avoid comparing all symbols of the edge by
   skipping its length immediately and thus save atomic operations - see
   Ukkonen's algorithm, skip trick).
   This function, in contradiction to the function trace_single_edge, 'sees' the
   whole picture, meaning it searches a string in the whole tree and not just in
   a specific edge.

   Input : The string, given in indice of the main string.

   Output: (by value) the node where tracing has stopped.
           (by reference) the edge position where last match occured, the string
	   position where last match occured, number of characters found, a flag
	   for signaling whether search is done.
*/

NODE* trace_string(
                      SUFFIX_TREE*    tree, 
                      /* Node to start from */
                      NODE*           node,         
                      /* String to trace */
                      PATH            str,         
                      /* Last matching position in edge */
                      DBL_WORD*       edge_pos,      
                      /* Last matching position in tree string */
                      DBL_WORD*       chars_found,
                      /* skip or not */
                      SKIP_TYPE       type)         
{
   /* This variable will be 1 when search is done.
      It is a return value from function trace_single_edge */
   int      search_done = 0;

   /* This variable will hold the number of matching characters found in the
      current edge. It is a return value from function trace_single_edge */
   DBL_WORD edge_chars_found;

   *chars_found = 0;

   while(search_done == 0)
   {
      *edge_pos        = 0;
      edge_chars_found = 0;
      node = trace_single_edge(tree, node, str, edge_pos, &edge_chars_found, type, &search_done);
      str.begin       += edge_chars_found;
      *chars_found    += edge_chars_found;
   }
   return node;
}

/******************************************************************************/
/*
   ST_FindSubstring :
   See suffix_tree.h for description.
*/

MATCH ST_FindSubstring(
                      /* The suffix array */
                      SUFFIX_TREE*    tree,      
                      /* The substring to find */
                      unsigned char*  W,         
                      /* The length of W */
                      DBL_WORD        P)         
{
   /* Starts with the root's son that has the first character of W as its
      incoming edge first character */
   NODE* node   = find_son(tree, tree->root, W[0]);
   DBL_WORD k,j = 0, node_label_end;
   MATCH currentMatch;
   currentMatch.length = 0;
   currentMatch.pos = 0;

   /* Scan nodes down from the root untill a leaf is reached or the substring is
      found */
   while(node!=0)
   {
      if(node->annot.optimisticMinMax == -1)
      {
      	return currentMatch;
      }
      if(node->annot.optimisticMinMax == tree->COST)
      {
      	// if(node->sons != NULL) return currentMatch;
         // if(tree->D[node->annot.optimisticTextPos] != -1){
            // fprintf(stderr,"node->annot.optimisticMinMax = %i, tree->D[node->annot.optimisticMinMax] = %i\n",node->annot.optimisticMinMax,tree->D[node->annot.optimisticTextPos]);
         if(tree->D[node->annot.optimisticTextPos] > currentMatch.length)
         {
            currentMatch.length = tree->D[node->annot.optimisticTextPos];
            currentMatch.pos = node->annot.optimisticTextPos;
         }
         return currentMatch;
         // }
         // else
         // {
         //    // fprintf(stderr,"NO D node->annot.optimisticMinMax = %i, tree->D[node->annot.optimisticMinMax] = %i\n",node->annot.optimisticMinMax,tree->D[node->annot.optimisticMinMax]);
         //    return currentMatch;
         // }
      }
      
      k = node->edge_label_start;
      node_label_end = get_node_label_end(tree,node);
      
      /* Scan a single edge - compare each character with the searched string */
      while(j<P && k<=node_label_end && tree->tree_string[k] == W[j])
      {
         j++;
         k++;
      }

      // j += node_label_end - k + 1;
      // k = node_label_end + 1;
      // if(node->annot.optimisticMinMax == tree->COST)
      // {
      // 	if(node->annot.distToC <= j) 
      // 	{
      // 	   j = node->annot.distToC - 1;
      // 	}
      // }
      
      currentMatch.length = j;
      if(node->annot.optimisticTextPos != 0)
      {
      	currentMatch.pos = node->annot.optimisticTextPos;
      }
      else
      {
        printf("Position of source was 0 during the search\n");
        exit(1);
      }
      
      /* Checking which of the stopping conditions are true */
      if(j == P)
      {
         /* W was found - it is a substring. Return its path starting index */
         return currentMatch;
      }
      else if(k > node_label_end)
         /* Current edge is found to match, continue to next edge */
         node = find_son(tree, node, W[j]);
      else
      {
         /* One non-matching symbols is found - W is not a substring */
         return currentMatch;
      }
   }
   return currentMatch;
}

NODE *getMinMaxOfChildren(NODE *node, SUFFIX_TREE *tree)
{
   NODE *resultSon = node->sons;
   NODE* currentSon = node->sons;
   while(currentSon != NULL)
   {
      //  // if(resultSon->annot.optimisticMinMax > currentSon->annot.optimisticMinMax)
      //  if(node->annot.optimisticMinMax == tree->COST)
      //  {
      //    if(currentSon->annot.optimisticTextPos == -1 || tree->D[currentSon->annot.optimisticTextPos] == -1){
      //       currentSon = currentSon->right_sibling;
      //       continue;
      //    }
      //    if(tree->D[resultSon->annot.optimisticTextPos] == -1) resultSon = currentSon;
      //    else if(tree->D[currentSon->annot.optimisticTextPos] > tree->D[resultSon->annot.optimisticTextPos] || (tree->D[resultSon->annot.optimisticTextPos] == tree->D[currentSon->annot.optimisticTextPos] && resultSon->annot.optimisticMinMax > currentSon->annot.optimisticMinMax))
      //    {
      //       resultSon = currentSon;
      //    }
      //  }
      //  else
      //  {
         if(resultSon->annot.optimisticMinMax > currentSon->annot.optimisticMinMax || (resultSon->annot.optimisticMinMax == currentSon->annot.optimisticMinMax && tree->D[resultSon->annot.optimisticTextPos] < tree->D[currentSon->annot.optimisticTextPos]))
         {
            resultSon = currentSon;
         }
      //  }
       currentSon = currentSon->right_sibling;
   }
   // if(resultSon->annot.optimisticMinMax == -1)
   // {
   //    fprintf(stderr,"ERROR: resultSon->annot.optimisticMinMax = -1\n");
   //    currentSon = node->sons;
   //    while(currentSon != NULL)
   //    {
   //       fprintf(stderr,"currentSon->annot.optimisticMinMax = %i\n",currentSon->annot.optimisticMinMax);
   //       currentSon = currentSon->right_sibling;
   //    }
   //    exit(1);
   // }
   return resultSon;
}

void changeAnnotationFromLeaf(unsigned int textPos, unsigned int finalPos, int len, unsigned int minMaxOfRange, unsigned int distToC, SUFFIX_TREE* tree)
{
   NODE * leaf = tree->inversePointers[textPos];
   // if(minMaxOfRange == tree->COST) 
   // {
   // 	if(leaf->annot.distToC > distToC)
   // 	   leaf->annot.distToC = distToC;
   // }
   if(minMaxOfRange > leaf->annot.minMax || leaf->annot.minMax == -1)
   {
	   leaf->annot.minMax = minMaxOfRange;
	   leaf->annot.optimisticMinMax = minMaxOfRange;
   }
   NODE *parent = leaf->father;
   while(parent != NULL && (int)parent->strDepth > len)
   {
   	NODE *newMinMaxHolder = getMinMaxOfChildren(parent, tree);
   	
   	unsigned int oldOptimisticMinMax = parent->annot.optimisticMinMax; 
   	if(textPos + parent->strDepth - 1 <= finalPos)
   	{ 
         unsigned cost = tree->costArray[cappedMax(tree->segm,textPos,textPos+parent->strDepth-1,tree->COST)];
// antes, usaca leaf->annot.optimisticMinMax como cost para decidir si asignarlo
// y luego asignaba el cappedMax
         if(parent->annot.minMax == tree->COST)
         {
            if(cost < tree->COST)
            {
               parent->annot.minMax = cost;
               parent->annot.textPos = textPos;
            }
            else
            {  
               if(tree->D[textPos] != -1 && (tree->D[textPos] > tree->D[parent->annot.textPos]))
               {
                  parent->annot.minMax = cost;
                  parent->annot.textPos = textPos;
               }
            }  
         }
         else 
   	   {
		      if(cost < parent->annot.minMax)
            {
               // fprintf(stderr, "cost = %i, parent->annot.minMax = %i\n",cost,parent->annot.minMax);
               parent->annot.minMax = cost;
               parent->annot.textPos = textPos;
            }
   	   }
   	}

      if(parent->annot.optimisticMinMax == -1)
      {
         parent->annot.optimisticMinMax = minMaxOfRange;
         parent->annot.optimisticTextPos = textPos; 
      }
      else 
      {
         if(parent->annot.optimisticMinMax == tree->COST)
         {
            if(newMinMaxHolder->annot.optimisticMinMax == tree->COST)
            {
               if(tree->D[newMinMaxHolder->annot.optimisticTextPos] > tree->D[parent->annot.optimisticTextPos])
               {
                  parent->annot.optimisticMinMax = newMinMaxHolder->annot.optimisticMinMax;
                  parent->annot.optimisticTextPos = newMinMaxHolder->annot.optimisticTextPos;
               }
               else
               {
                  parent->annot.optimisticMinMax = parent->annot.minMax;
                  parent->annot.optimisticTextPos = parent->annot.textPos;
               }
            }
            else
            {
               // fprintf(stderr,"ERROR %d vs %d\n", parent->annot.optimisticMinMax, newMinMaxHolder->annot.optimisticMinMax);
               parent->annot.optimisticMinMax = newMinMaxHolder->annot.optimisticMinMax;
               parent->annot.optimisticTextPos = newMinMaxHolder->annot.optimisticTextPos;
            }
         }
         else
         {
            if(newMinMaxHolder->annot.optimisticMinMax < parent->annot.minMax)
            {
               parent->annot.optimisticMinMax = newMinMaxHolder->annot.optimisticMinMax;
               parent->annot.optimisticTextPos = newMinMaxHolder->annot.optimisticTextPos;
            
            }
            else
            {
               // if(newMinMaxHolder->annot.optimisticMinMax == -1)
               // {
               //    fprintf(stderr,"ERROR: newMinMaxHolder->annot.optimisticMinMax = %i, parent->annot.minMax = %i\n",newMinMaxHolder->annot.optimisticMinMax,parent->annot.minMax);
               //    exit(1);
               // }
               parent->annot.optimisticMinMax = parent->annot.minMax;
               parent->annot.optimisticTextPos = parent->annot.textPos; 
            
            }
         }
      }
   	// if(tree->D[textPos] > tree->D[parent->annot.textPos] || (tree->D[textPos] == tree->D[parent->annot.textPos] && newMinMaxHolder->annot.optimisticMinMax < parent->annot.minMax))
   	// {
   	// 	parent->annot.optimisticMinMax = newMinMaxHolder->annot.optimisticMinMax;
   	// 	parent->annot.optimisticTextPos = newMinMaxHolder->annot.optimisticTextPos;
   	// }
   	// else
   	// {
   	// 	parent->annot.optimisticMinMax = parent->annot.minMax;
   	// 	parent->annot.optimisticTextPos = parent->annot.textPos; 
   	// }

	   int newOptimisticMinMax = parent->annot.optimisticMinMax;
   	parent = parent->father;
      // if(parent == NULL || (tree->D[parent->annot.optimisticTextPos] > tree->D[newMinMaxHolder->annot.optimisticTextPos] && (newOptimisticMinMax >= parent->annot.minMax && (oldOptimisticMinMax == newOptimisticMinMax)))) return;
	   // if(parent == NULL || (newOptimisticMinMax >= parent->annot.minMax && (oldOptimisticMinMax == newOptimisticMinMax) && (textPos + parent->strDepth - 1 <= finalPos-len))){
      //    // fprintf(stderr, "STOPPING EARLY\n");
      //    return;
      // } 
	// este fix de arriba puede funcionar, pero no se si es lo mejor
	// la leaf no tiene el mejor valor, lo tiene el hijo, por su RMQ
   	// creo que aunque nada mejore tengo que seguir al parent porque
	// el saca su resultado del RMQ. lo lamento por los tiempos!
	// version que no funcionaba:
	// if(parent == NULL || (leaf->annot.optimisticMinMax >= parent->annot.minMax && (oldOptimisticMinMax == newOptimisticMinMax))) return;
   }  
}


void propagateAnnotation(unsigned int textPos, unsigned int len, SUFFIX_TREE* tree)
{
   unsigned int currentMinMaxOfRange = 0, distToC = 0, i;
   for(i = textPos+len; i > 0; i--)
   {
      if(currentMinMaxOfRange < tree->costArray[i])
      {
      	currentMinMaxOfRange = tree->costArray[i];
      }
      if(tree->maxStrDepth[i] < textPos) break;
      changeAnnotationFromLeaf(i, textPos+len, (int)textPos-i, currentMinMaxOfRange, distToC, tree);
   }
  // fprintf(stderr,"%i ",textPos-i); fflush(stderr);
}


int parseBLZ(SUFFIX_TREE *tree)
{
   unsigned int textPos = 1;
   int z = 0;
   unsigned int positionOfPreviousC = 0;
   printf("n = %d\n",tree->length);
   while(textPos <= tree->length)
   {
      MATCH currentPhrase = ST_FindSubstring(tree, (unsigned char*)tree->tree_string + textPos, tree->length);
      z++;
      unsigned int k = 0, i;
   if (textPos/1024/1024 != (textPos+currentPhrase.length+1)/1024/1024)
   {  
      fprintf(stderr,"%i MB\n",(textPos+currentPhrase.length+1)/1024/1024); }
      for(i = 0; i < currentPhrase.length; i++)
      {
      	tree->costArray[textPos+i] = tree->costArray[currentPhrase.pos + k] + 1;
         if(tree->costArray[textPos+i] == tree->COST)
         {
            int currentPos;
            tree->D[textPos+i] = 0;
            for(currentPos = textPos+i-1; currentPos > positionOfPreviousC; currentPos--)
            {
               tree->D[currentPos] = tree->D[currentPos+1] + 1;
            }  
            positionOfPreviousC = textPos+i;
         }
         // printf("costArray[%i] = %i\n", textPos+i, tree->costArray[textPos+i]);
         segmUpdate(tree->segm,textPos+i,tree->costArray[textPos+i]);
         if (tree->costArray[textPos+i] > tree->COST) 
         { fprintf(stderr,"U[%i] = %i\n",textPos+i,tree->costArray[textPos+i]); exit(1); }
      	k++;
      	if(currentPhrase.pos + k == textPos) k = 0;
      }
      tree->costArray[textPos+currentPhrase.length] = 0;

      segmUpdate(tree->segm,textPos+currentPhrase.length,0);
      propagateAnnotation(textPos, currentPhrase.length, tree);
      
      // // check if generated phrase is correct: tree->string[textPos..textPos+currentPhrase.length-1] == tree->string[currentPhrase.pos..currentPhrase.pos+currentPhrase.length-1]
      // // if not, print error message and exit
      // if(memcmp(tree->tree_string+textPos, tree->tree_string+currentPhrase.pos, currentPhrase.length) != 0)
      // {
      //    fprintf(stderr,"Error: Generated phrase is incorrect\n");
      //    fprintf(stderr,"textPos = %i, currentPhrase.pos = %i, currentPhrase.length = %i\n", textPos, currentPhrase.pos, currentPhrase.length);
      //    exit(1);
      // }

      textPos = textPos+currentPhrase.length+1;
      printf("(%d,%d,%d)\n", currentPhrase.pos-1, currentPhrase.length, (unsigned)tree->tree_string[textPos-1]);

   }
   printf("\n\nz = %i phrases\n",z);
   // unsigned int j;
   // for(j = 1; j < textPos; j++)
   // {
   // 	printf("%d ", tree->D[j]);
   // }
   // printf("\n");
   return z;
}

/******************************************************************************/
/*
   follow_suffix_link :
   Follows the suffix link of the source node according to Ukkonen's rules. 

   Input : The tree, and pos. pos is a combination of the source node and the 
           position in its incoming edge where suffix ends.
   Output: The destination node that represents the longest suffix of node's 
           path. Example: if node represents the path "abcde" then it returns 
           the node that represents "bcde".
*/

void follow_suffix_link(SUFFIX_TREE* tree, POS* pos)
{
   /* gama is the string between node and its father, in case node doesn't have
      a suffix link */
   PATH      gama;            
   /* dummy argument for trace_string function */
   DBL_WORD  chars_found = 0;   
   
   if(pos->node == tree->root)
   {
      return;
   }

   /* If node has no suffix link yet or in the middle of an edge - remember the
      edge between the node and its father (gama) and follow its father's suffix
      link (it must have one by Ukkonen's lemma). After following, trace down 
      gama - it must exist in the tree (and thus can use the skip trick - see 
      trace_string function description) */
   if(pos->node->suffix_link == 0 || is_last_char_in_edge(tree,pos->node,pos->edge_pos) == 0)
   {
      /* If the node's father is the root, than no use following it's link (it 
         is linked to itself). Tracing from the root (like in the naive 
         algorithm) is required and is done by the calling function SEA uppon 
         recieving a return value of tree->root from this function */
      if(pos->node->father == tree->root)
      {
         pos->node = tree->root;
         return;
      }
      
      /* Store gama - the indices of node's incoming edge */
      gama.begin      = pos->node->edge_label_start;
      gama.end      = pos->node->edge_label_start + pos->edge_pos;
      /* Follow father's suffix link */
      pos->node      = pos->node->father->suffix_link;
      /* Down-walk gama back to suffix_link's son */
      pos->node      = trace_string(tree, pos->node, gama, &(pos->edge_pos), &chars_found, skip);
   }
   else
   {
      /* If a suffix link exists - just follow it */
      pos->node      = pos->node->suffix_link;
      pos->edge_pos   = get_node_label_length(tree,pos->node)-1;
   }
}

/******************************************************************************/
/*
   create_suffix_link :
   Creates a suffix link between node and the node 'link' which represents its 
   largest suffix. The function could be avoided but is needed to monitor the 
   creation of suffix links when debuging or changing the tree.

   Input : The node to link from, the node to link to.

   Output: None.
*/

void create_suffix_link(NODE* node, NODE* link)
{
   node->suffix_link = link;
}

/******************************************************************************/
/*
   SEA :
   Single-Extension-Algorithm (see Ukkonen's algorithm). Ensure that a certain 
   extension is in the tree.

   1. Follows the current node's suffix link.
   2. Check whether the rest of the extension is in the tree.
   3. If it is - reports the calling function SPA of rule 3 (= current phase is 
      done).
   4. If it's not - inserts it by applying rule 2.

   Input : The tree, pos - the node and position in its incoming edge where 
           extension begins, str - the starting and ending indices of the 
           extension, a flag indicating whether the last phase ended by rule 3 
           (last extension of the last phase already existed in the tree - and 
           if so, the current phase starts at not following the suffix link of 
           the first extension).

   Output: The rule that was applied to that extension. Can be 3 (phase is done)
           or 2 (a new leaf was created).
*/

void SEA(
                      SUFFIX_TREE*   tree, 
                      POS*           pos,
                      PATH           str, 
                      DBL_WORD*      rule_applied,
                      unsigned char           after_rule_3)
{
   DBL_WORD   chars_found = 0 , path_pos = str.begin;
   NODE*      tmp;
 
#ifdef DEBUG   
   ST_PrintTree(tree);
   printf("extension: %lu  phase+1: %lu",str.begin, str.end);
   if(after_rule_3 == 0)
      printf("   followed from (%lu,%lu | %lu) ", pos->node->edge_label_start, get_node_label_end(tree,pos->node), pos->edge_pos);
   else
      printf("   starting at (%lu,%lu | %lu) ", pos->node->edge_label_start, get_node_label_end(tree,pos->node), pos->edge_pos);
#endif

#ifdef STATISTICS
   counter++;
#endif

   /* Follow suffix link only if it's not the first extension after rule 3 was applied */
   if(after_rule_3 == 0)
      follow_suffix_link(tree, pos);

#ifdef DEBUG   
#ifdef STATISTICS
   if(after_rule_3 == 0)
      printf("to (%lu,%lu | %lu). counter: %lu\n", pos->node->edge_label_start, get_node_label_end(tree,pos->node),pos->edge_pos,counter);
   else
      printf(". counter: %lu\n", counter);
#endif
#endif

   /* If node is root - trace whole string starting from the root, else - trace last character only */
   if(pos->node == tree->root)
   {
      pos->node = trace_string(tree, tree->root, str, &(pos->edge_pos), &chars_found, no_skip);
   }
   else
   {
      str.begin = str.end;
      chars_found = 0;

      /* Consider 2 cases:
         1. last character matched is the last of its edge */
      if(is_last_char_in_edge(tree,pos->node,pos->edge_pos))
      {
         /* Trace only last symbol of str, search in the  NEXT edge (node) */
         tmp = find_son(tree, pos->node, tree->tree_string[str.end]);
         if(tmp != 0)
         {
            pos->node      = tmp;
            pos->edge_pos   = 0;
            chars_found      = 1;
         }
      }
      /* 2. last character matched is NOT the last of its edge */
      else
      {
         /* Trace only last symbol of str, search in the CURRENT edge (node) */
         if(tree->tree_string[pos->node->edge_label_start+pos->edge_pos+1] == tree->tree_string[str.end])
         {
            pos->edge_pos++;
            chars_found   = 1;
         }
      }
   }

   /* If whole string was found - rule 3 applies */
   if(chars_found == str.end - str.begin + 1)
   {
      *rule_applied = 3;
      /* If there is an internal node that has no suffix link yet (only one may 
         exist) - create a suffix link from it to the father-node of the 
         current position in the tree (pos) */
      if(suffixless != 0)
      {
         create_suffix_link(suffixless, pos->node->father);
         /* Marks that no internal node with no suffix link exists */
         suffixless = 0;
      }

      #ifdef DEBUG   
         printf("rule 3 (%lu,%lu)\n",str.begin,str.end);
      #endif
      return;
   }
   
   /* If last char found is the last char of an edge - add a character at the 
      next edge */
   if(is_last_char_in_edge(tree,pos->node,pos->edge_pos) || pos->node == tree->root)
   {
      /* Decide whether to apply rule 2 (new_son) or rule 1 */
      if(pos->node->sons != 0)
      {
         /* Apply extension rule 2 new son - a new leaf is created and returned 
            by apply_extension_rule_2 */
         apply_extension_rule_2(pos->node, str.begin+chars_found, str.end, path_pos, 0, new_son);
         *rule_applied = 2;
         /* If there is an internal node that has no suffix link yet (only one 
            may exist) - create a suffix link from it to the father-node of the 
            current position in the tree (pos) */
         if(suffixless != 0)
         {
            create_suffix_link(suffixless, pos->node);
            /* Marks that no internal node with no suffix link exists */
            suffixless = 0;
         }
      }
   }
   else
   {
      /* Apply extension rule 2 split - a new node is created and returned by 
         apply_extension_rule_2 */
      tmp = apply_extension_rule_2(pos->node, str.begin+chars_found, str.end, path_pos, pos->edge_pos, split);
      if(suffixless != 0)
         create_suffix_link(suffixless, tmp);
      /* Link root's sons with a single character to the root */
      if(get_node_label_length(tree,tmp) == 1 && tmp->father == tree->root)
      {
         tmp->suffix_link = tree->root;
         /* Marks that no internal node with no suffix link exists */
         suffixless = 0;
      }
      else
         /* Mark tmp as waiting for a link */
         suffixless = tmp;
      
      /* Prepare pos for the next extension */
      pos->node = tmp;
      *rule_applied = 2;
   }
}

/******************************************************************************/
/*
   SPA :
   Performs all insertion of a single phase by calling function SEA starting 
   from the first extension that does not already exist in the tree and ending 
   at the first extension that already exists in the tree. 

   Input :The tree, pos - the node and position in its incoming edge where 
          extension begins, the phase number, the first extension number of that
          phase, a flag signaling whether the extension is the first of this 
          phase, after the last phase ended with rule 3. If so - extension will 
          be executed again in this phase, and thus its suffix link would not be
          followed.

   Output:The extension number that was last executed on this phase. Next phase 
          will start from it and not from 1
*/

void SPA(
                      /* The tree */
                      SUFFIX_TREE*    tree,            
                      /* Current node */
                      POS*            pos,            
                      /* Current phase number */
                      DBL_WORD        phase,            
                      /* The last extension performed in the previous phase */
                      DBL_WORD*       extension,         
                      /* 1 if the last rule applied is 3 */
                      unsigned char*           repeated_extension)   
{
   /* No such rule (0). Used for entering the loop */
   DBL_WORD   rule_applied = 0;   
   PATH       str;
   
   /* Leafs Trick: Apply implicit extensions 1 through prev_phase */
   tree->e = phase+1;

   /* Apply explicit extensions untill last extension of this phase is reached 
      or extension rule 3 is applied once */
   while(*extension <= phase+1)            
   {
      str.begin       = *extension;
      str.end         = phase+1;
      
      /* Call Single-Extension-Algorithm */
      SEA(tree, pos, str, &rule_applied, *repeated_extension);
      
      /* Check if rule 3 was applied for the current extension */
      if(rule_applied == 3)
      {
         /* Signaling that the next phase's first extension will not follow a 
            suffix link because same extension is repeated */
         *repeated_extension = 1;
         break;
      }
      *repeated_extension = 0;
      (*extension)++;
   }
   return;
}


unsigned dfsForInversePointers(NODE * node, SUFFIX_TREE *tree, unsigned int depth)
{ unsigned num = 0;
	node->annot.minMax = -1;
	node->annot.optimisticMinMax = -1;
	// node->annot.distToC = -1;
#ifdef PREFIXSUM
   node->annot.totalCost = -1;
#endif
	node->strDepth = depth;
	if(node->sons == NULL)
	{
		tree->inversePointers[node->path_position] = node;
		tree->maxStrDepth[node->path_position] = node->path_position + node->father->strDepth - 1;
   		node->annot.optimisticTextPos = node->path_position;
   		node->annot.textPos = node->path_position;
   		num++;
	}
	else
	{
		node->annot.textPos = 0;
		node->annot.optimisticTextPos = 0;
		NODE *currentChild = node->sons;
		while(currentChild != NULL)
		{
			num += dfsForInversePointers(currentChild, tree, depth + (currentChild->edge_label_end - currentChild->edge_label_start) + 1);
			currentChild = currentChild->right_sibling;
		}
		
	}
  return num;
}

/******************************************************************************/
/*
   ST_CreateTree :
   Allocates memory for the tree and starts Ukkonen's construction algorithm by 
   calling SPA n times, where n is the length of the source string.

   Input : The source string and its length. The string is a sequence of 
           unsigned characters (maximum of 256 different symbols) and not 
           null-terminated. The only symbol that must not appear in the string 
           is $ (the dollar sign). It is used as a unique symbol by the 
           algorithm ans is appended automatically at the end of the string (by 
           the program, not by the user!). The meaning of the $ sign is 
           connected to the implicit/explicit suffix tree transformation, 
           detailed in Ukkonen's algorithm.

   Output: A pointer to the newly created tree. Keep this pointer in order to 
           perform operations like search and delete on that tree. Obviously, no
	   de-allocating of the tree space could be done if this pointer is 
	   lost, as the tree is allocated dynamically on the heap.
*/

SUFFIX_TREE* ST_CreateTree(unsigned char* str, DBL_WORD length)
{
   SUFFIX_TREE*  tree;
   DBL_WORD      phase , extension;
   unsigned char          repeated_extension = 0;
   POS           pos;
   

   if(str == 0)
      return 0;

   /* Allocating the tree */
   tree = malloc(sizeof(SUFFIX_TREE));
   if(tree == 0)
   {
      printf("\nOut of memory.\n");
      exit(0);
   }
   heap+=sizeof(SUFFIX_TREE);
   tree->inversePointers = malloc(sizeof(NODE *) * (length + 2));
   tree->costArray = malloc(sizeof(unsigned int) * (length + 2));
   tree->maxStrDepth = malloc(sizeof(unsigned int) * (length + 2));
#ifdef PREFIXSUM
   tree->prefixSumCostArray = malloc(sizeof(unsigned int) * (length + 2));
   tree->prefixSumCostArray[0] = 0;
   tree->prefixSumCostArray[1] = 0;
   tree->prefixSumCostArray[2] = 0;
#endif
   { int i;
     for (i=0;i<=length+1;i++) tree->costArray[i] = length+1;
   }
   tree->segm = segmCreate(tree->costArray,length+1);

   /* Calculating string length (with an ending $ sign) */
   tree->length         = length+1;
   ST_ERROR            = length+10;
   
   /* Allocating the only real string of the tree */
   tree->tree_string = str-1; // -1 to point to it 1-based
      /* why allocating twice? it has already len+1 length 
		        malloc((tree->length+1)*sizeof(unsigned char));
   if(tree->tree_string == 0)
   {
      printf("\nOut of memory.\n");
      exit(0);
   } 
   memcpy(tree->tree_string+sizeof(unsigned char),str,length*sizeof(unsigned char));
   /* $ is considered a uniqe symbol * /
   tree->tree_string[tree->length] = 0; // replaced by a zero '$';
     */ 
   
   /* Allocating the tree root node */
   tree->root            = create_node(0, 0, 0, 0);
   tree->root->suffix_link = 0;

   /* Initializing algorithm parameters */
   extension = 2;
   phase = 2;
   
   /* Allocating first node, son of the root (phase 0), the longest path node */
   tree->root->sons = create_node(tree->root, 1, tree->length, 1);
   suffixless       = 0;
   pos.node         = tree->root;
   pos.edge_pos     = 0;

   /* Ukkonen's algorithm begins here */
   for(; phase < tree->length; phase++)
   {
      /* Perform Single Phase Algorithm */
      SPA(tree, &pos, phase, &extension, &repeated_extension);
   }
   
   unsigned nn = dfsForInversePointers(tree->root, tree, 0);
   if (nn != tree->length) 
	fprintf(stderr,"text length = %i, suffix tree leaves = %i\n",tree->length,nn);
   else fprintf(stderr,"dfs matches\n");
   unsigned int i;
   for(i = 2; i <= tree->length; i++)
   {
   	if(tree->maxStrDepth[i-1] > tree->maxStrDepth[i])
   		tree->maxStrDepth[i] = tree->maxStrDepth[i-1];
   }


   /* Initialize D array to -1 */
   tree->D = malloc(sizeof(int) * (tree->length + 1));
   for(i = 0; i <= tree->length; i++)
   {
   	tree->D[i] = -1;
   }
   return tree;
}

/******************************************************************************/
/*
   ST_DeleteSubTree :
   Deletes a subtree that is under node. It recoursively calls itself for all of
   node's right sons and then deletes node.

  Input : The node that is the root of the subtree to be deleted.

  Output: None.
*/

void ST_DeleteSubTree(NODE* node)
{
   /* Recoursion stoping condition */
   if(node == 0)
      return;
   /* Recoursive call for right sibling */
   if(node->right_sibling!=0)
      ST_DeleteSubTree(node->right_sibling);
   /* Recoursive call for first son */
   if(node->sons!=0)
      ST_DeleteSubTree(node->sons);
   /* Delete node itself, after its whole tree was deleted as well */
   free(node);
}

/******************************************************************************/
/*
   ST_DeleteTree :
   Deletes a whole suffix tree by starting a recoursive call to ST_DeleteSubTree
   from the root. After all of the nodes have been deleted, the function deletes
   the structure that represents the tree.

   Input : The tree to be deleted.

   Output: None.
*/

void ST_DeleteTree(SUFFIX_TREE* tree)
{
   if(tree == 0)
      return;
   ST_DeleteSubTree(tree->root);
   free(tree);
}

/******************************************************************************/
/*
   ST_PrintNode :
   Prints a subtree under a node of a certain tree-depth.

   Input : The tree, the node that is the root of the subtree, and the depth of 
           that node. The depth is used for printing the branches that are 
           coming from higher nodes and only then the node itself is printed. 
           This gives the effect of a tree on screen. In each recoursive call, 
           the depth is increased.
  
   Output: A printout of the subtree to the screen.
*/

void ST_PrintNode(SUFFIX_TREE* tree, NODE* node1, long depth)
{
   NODE* node2 = node1->sons;
   long  d = depth , start = node1->edge_label_start , end;
   end     = get_node_label_end(tree, node1);

   if(depth>0)
   {
      /* Print the branches coming from higher nodes */
      while(d>1)
      {
         printf("|");
         d--;
      }
      printf("+");
      /* Print the node itself */
      while(start<=end)
      {
         printf("%c",tree->tree_string[start]);
         start++;
      }
      #ifdef DEBUG
         printf("  \t\t\t(%lu,%lu | %lu)",node1->edge_label_start,end,node1->path_position);
      #endif
      printf("\n");
   }
   /* Recoursive call for all node1's sons */
   while(node2!=0)
   {
      ST_PrintNode(tree,node2, depth+1);
      node2 = node2->right_sibling;
   }
}

/******************************************************************************/
/*
   ST_PrintFullNode :
   This function prints the full path of a node, starting from the root. It 
   calls itself recoursively and than prints the last edge.

   Input : the tree and the node its path is to be printed.

   Output: Prints the path to the screen, no return value.
*/

void ST_PrintFullNode(SUFFIX_TREE* tree, NODE* node)
{
   long start, end;
   if(node==NULL)
      return;
   /* Calculating the begining and ending of the last edge */
   start   = node->edge_label_start;
   end     = get_node_label_end(tree, node);
   
   /* Stoping condition - the root */
   if(node->father!=tree->root)
      ST_PrintFullNode(tree,node->father);
   /* Print the last edge */
   while(start<=end)
   {
      printf("%c",tree->tree_string[start]);
      start++;
   }
}


/******************************************************************************/
/*
   ST_PrintTree :
   This function prints the tree. It simply starts the recoursive function 
   ST_PrintNode with depth 0 (the root).

   Input : The tree to be printed.
  
   Output: A print out of the tree to the screen.
*/

void ST_PrintTree(SUFFIX_TREE* tree)
{
   printf("\nroot\n");
   ST_PrintNode(tree, tree->root, 0);
}

/******************************************************************************/
/*
   ST_SelfTest :
   Self test of the tree - search for all substrings of the main string. See 
   testing paragraph in the readme.txt file.

   Input : The tree to test.

   Output: 1 for success and 0 for failure. Prints a result message to the screen.
*/


DBL_WORD ST_SelfTest(SUFFIX_TREE* tree)
{
   DBL_WORD k,j;

#ifdef STATISTICS
   DBL_WORD old_counter = counter;
#endif

   /* Loop for all the prefixes of the tree source string */
   for(k = 1; k<tree->length; k++)
   {
      /* Loop for each suffix of each prefix */
      for(j = 1; j<=k; j++)
      {
#ifdef STATISTICS
         counter = 0;
#endif
         /* Search the current suffix in the tree */
         MATCH m = ST_FindSubstring(tree, (unsigned char*)(tree->tree_string+j), k-j+1);
         if(m.length == 0)
         {
            printf("\n\nTest Results: Fail in string (%lu,%lu).\n\n",j,k);
            return 0;
         }
      }
   }
#ifdef STATISTICS
   counter = old_counter;
#endif
   /* If we are here no search has failed and the test passed successfuly */
   printf("\n\nTest Results: Success.\n\n");
   return 1;
}




int main(int argc, char* argv[])
{
	SUFFIX_TREE* tree;
	unsigned char command, *str = NULL, *filename, freestr = 0;
	FILE* file = 0;
	DBL_WORD i,z,len = 0;

	if(argc < 3) {
	   fprintf(stderr,"Usage: %s <filename> <maxc>\n",argv[0]); 
	   exit(1);
	}
   filename = argv[1];
   file = fopen(filename,"r");
   /*Check for validity of the file.*/
   if(file == 0)
   {
      printf("can't open file.\n");
      return(0);
   }
   /*Calculate the file length in bytes. This will be the length of the source string.*/
   fseek(file, 0, SEEK_END);
   len = ftell(file);
   fseek(file, 0, SEEK_SET);
   str = (unsigned char*)malloc((len+1)*sizeof(unsigned char));
   if(str == 0)
   {
      printf("\nOut of memory.\n");
      exit(0);
   }
   fread(str, sizeof(unsigned char), len, file);
   fclose(file);
   { 
      unsigned i;
      for (i=0;i<len;i++)
         if (str[i] == 0)
         {
            fprintf(stderr,"Cannot process this string, it has zeros inside\n");
            exit(1);
         }
   }
   str[len] = 0;

	fprintf(stderr,"Constructing tree...\n");
	tree = ST_CreateTree(str,len); // it appends the 0 anyway
	fprintf(stderr,"Parsing...\n");
   tree->COST = atoi(argv[2]);
	char *filename_cost = (char*)malloc(strlen(filename)+45);
	strcpy(filename_cost,filename);
	char *strCost = (char *)malloc(sizeof(char)*10);
	sprintf(strCost, "%d", tree->COST);
	strcat(filename_cost,"_greedier");
	strcat(filename_cost,strCost);
	strcat(filename_cost,".cost");
	fprintf(stderr, "filename_cost: %s\n",filename_cost);
	z = parseBLZ(tree);
	fprintf(stderr,"%i phrases\n",z);
	
   free(str);
   free(filename_cost);
   free(strCost);
   ST_DeleteTree(tree);
   
	return 0;
}
