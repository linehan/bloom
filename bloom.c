/******************************************************************************
 * bloom.c
 * ```````
 * Bloom filters
 *
 * HISTORY 
 *                                                   {x,  y,  z}
 * A Bloom filter is a probibalistic                  :   :   :
 * data structure with several interesting           /|\ /|\ /|\
 * properties, such as low memory usage,            / | X | X | \
 * asymmetric query confidence, and a very         /  |/ \|/ \|  \
 * speedy O(k) membership test.                   /   |   |   \   \
 *                                               /   /|  /|\  |\   \
 * Because a Bloom filter can                   .   . . . . . . .   .
 * accept any input that can be       00000000001000101010101010100010000000000
 * hashed effectively (such as                       "    "    " 
 * strings), that membership test                     \   |   /
 * tends to draw a crowd. TNSTAAFL, but                \  |  /
 * as caveats go, the Bloom filters' are                \ | / 
 * more interesting than incapacitating.                 \|/
 *                                                        : 
 * Most notably, it can tell you with certainty          {w}
 * that an item 'i' is *not* a member of set 's', 
 * but it can only tell you with some finite 
 * probability whether an item 'i' *is* a member
 * of set 's'.
 *
 * Still, along with the intriguing possibility of using bitwise AND and OR 
 * to compute the logical union and intersection of two filters, the cheap
 * cost of adding elements to the filter set, and the low memory requirements,
 * the Bloom filter is a good choice for many applications.
 *
 * NOTES
 *
 * Let's look more closely at the probability values.
 *
 * Assume that a hash function selects each array position with equal
 * probability. If m is the number of bits in the array, and k is the number
 * of hash functions, then the probability that a certain bit is not set
 * to 1 by a certain hash function during the insertion of an element is
 *
 *      1-(1/m).
 *
 * The probability that it is not set to 1 by any of the hash functions is
 *
 *      (1-(1/m))^k.
 *
 * If we have inserted n elements, the probability that a certain bit is
 * set 0 is
 *
 *      (1-(1/m))^kn,
 *
 * Meaning that the probability said bit is set to 1 is therefore
 *
 *      1-([1-(1/m)]^kn).
 *
 * Now test membership of an element that is not in the set. Each of the k
 * array positions computed by the hash functions is 1 with a probability
 * as above. The probability of all of them being 1, which would cause the
 * algorithm to erroneously claim that the element is in the set, is often
 * given as
 *
 *      (1-[1-(1/m)]^kn)^k ~~ (1 - e^(-kn/m))^k.
 *
 * This is not strictly correct as it assumes independence for the 
 * probabilities of each bit being set. However, assuming it is a close
 * approximation we have that the probability of false positives descreases
 * as m (the number of bits in the array) increases, and increases as n
 * (the number of inserted elements) increases. For a given m and n, the
 * value of k (the number of hash functions) that minimizes the probability
 * is
 *
 *      (m/n)ln(2) ~~ 0.7(m/n),
 *
 * which gives the false positive probability of
 *
 *      2^-k ~~ 0.6185^(m/n).
 *
 * The required number of bits m, given n and a desired false positive
 * probability p (and assuming the optimal value of k is used) can be
 * computed by substituting the optimal value of k in the probability
 * expression above:
 *
 *      p = (1 - e^(-(((m/n)ln(2))*(n/m))))^((m/n)ln(2)),
 *
 * which simplifies to
 *
 *      ln(p) = -(m/n) * (ln2)^2.
 *
 * This results in the equation
 *
 *      m = -((n*ln(p)) / ((ln(2))^2))
 *
 * The classic filter uses 
 *
 *       1.44*log2(1/eta)
 *      
 * bits of space per inserted key, where eta is the false positive rate of
 * the Bloom filter.
 *
 * AUTHOR
 * Jason Linehan (patientulysses@gmail.com)
 * 
 * LICENSE
 * Public domain.
 *
 ******************************************************************************/

#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>

#include "bloom.h"


#define SETBIT(a,n) (a[n/CHAR_BIT] |= (1<<(n%CHAR_BIT)))
#define GETBIT(a,n) (a[n/CHAR_BIT] &  (1<<(n%CHAR_BIT)))
#define ROUND(size) ((size + CHAR_BIT - 1) / CHAR_BIT)


/******************************************************************************
 * bloom_new  Allocate and return a pointer to a new Bloom filter.
 * `````````
 * @size  : size of the bit array in the filter
 * @nfuncs: the number of hash functions 
 * Returns: An allocated bloom filter 
 *
 * USAGE
 * For best results, make 'size' a power of 2.
 *
 ******************************************************************************/
struct bloom_t *bloom_new(size_t size, size_t num_hashes, ...)
{
        struct bloom_t *bloom;
        va_list hashes;
        int n;

        /* Allocate Bloom filter container */
        if (!(bloom = malloc(sizeof(struct bloom_t))))
                return NULL;

        /* Allocate Bloom array */
        if (!(bloom->a = calloc(ROUND(size), sizeof(char)))) {
                free(bloom);
                return NULL;
        }

        /* Allocate Bloom filter hash function pointers */
        if (!(bloom->hash = (hashfp_t*)malloc(num_hashes*sizeof(hashfp_t)))) {
                free(bloom->a);
                free(bloom);
                return NULL;
        }

        /* Assign hash functions to pointers in the Bloom filter */
        va_start(hashes, num_hashes);

                for (n=0; n<num_hashes; n++)
                        bloom->hash[n] = va_arg(hashes, hashfp_t);

        va_end(hashes);

        /* 
         * Record the number of hash functions (k) and the number of bytes
         * in the Bloom array (m).
         */
        bloom->k = num_hashes;
        bloom->m = size;

        return bloom;
}


/******************************************************************************
 * bloom_del  Delete a Bloom filter.
 * `````````
 * @bloom : The condemned. 
 * Returns: nothing. 
 *
 ******************************************************************************/
void bloom_del(struct bloom_t *bloom)
{
        free(bloom->a);
        free(bloom->hash);
        free(bloom);
}


/******************************************************************************
 * bloom_add  Add a string to a Bloom filter.
 * `````````
 * @bloom : Bloom filter
 * @s     : string to add
 * Returns: nothing. 
 *
 * CAVEAT 
 * Once a string has been added to the filter, it cannot be "removed"!
 *
 ******************************************************************************/
void bloom_add(struct bloom_t *bloom, const char *s)
{
        unsigned int hash;
        int n;

        for (n=0; n<bloom->k; n++) {
                hash = (unsigned int)bloom->hash[n](s);
                SETBIT(bloom->a, (hash % bloom->m));
        }
}


/******************************************************************************
 * bloom_check  Determine if a string is in the Bloom filter. 
 * ```````````
 * @bloom : Bloom filter
 * @s     : string to add
 * Returns: false if string does not exist in the filter, otherwise true. 
 *
 * NOTES
 *
 * So this is the freakshow that bored programmers pay a nickel to get a
 * peek at, step right up. This is the way the membership test works.
 *
 * The string 's' is hashed once for each of the 'k' hash functions, as 
 * though we were planning to add it to the filter. Instead of adding it
 * however, we examine the bit that we *would* have set, and consider its
 * value.
 *
 * If the bit is 1 (set), the string we are hashing may be in the filter,
 * since it would have set this bit when it was originally hashed. However,
 * it may also be that another string just happened to produce a hash value
 * that would also set this bit. That would be a false positive. This is why 
 * we have k > 1, so we can minimize the likelihood of false positives 
 * occuring.
 *
 * If every bit corresponding to every one of the k hashes of our query 
 * string is set, we can say with some probability of being correct that 
 * the string we are holding is indeed "in" the filter. However, we can 
 * never be sure. 
 *
 * If, however, as we hash our string and peek at the resulting bit in the
 * filter, we find the bit is 0 (not set)... well now, that's different. 
 * In this case, we can say with absolute certainty that the string we are 
 * holding is *not* in the filter, because if it were, this bit would have 
 * to be set.
 * 
 * In this way, the Bloom filter can answer NO with absolute surety, but
 * can only speak a qualified YES.
 *
 ******************************************************************************/
bool bloom_check(struct bloom_t *bloom, const char *s)
{
        unsigned int hash;
        int n;

        for (n=0; n<bloom->k; n++) {
                hash = (unsigned int)bloom->hash[n](s);
                if (!(GETBIT(bloom->a, (hash % bloom->m))))
                        return false;
        }
        return true; /* ? */
}

