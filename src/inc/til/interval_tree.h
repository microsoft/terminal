// Code here is taken from the Geeks for Geeks website and edited

#pragma once

namespace til
{
    class IntervalTree
    {
    public:
        // Structure to represent an interval
        struct Interval
        {
            COORD low, high;
        };

        // Structure to represent a node in Interval Search Tree
        struct ITNode
        {
            Interval* i;
            COORD max;
            ITNode *left, *right;
            size_t patternId;
        };

        // A utility function to create a new Interval Search Tree Node
        ITNode* newNode(Interval i, size_t id)
        {
            ITNode* temp = new ITNode;
            temp->i = new Interval(i);
            temp->max = i.high;
            temp->left = temp->right = NULL;
            temp->patternId = id;
            return temp;
        };

        // A utility function to insert a new Interval Search Tree Node
        // This is similar to BST Insert.  Here the low value of interval
        // is used to maintain BST property
        ITNode* insert(ITNode* root, Interval i, size_t id)
        {
            // Base case: Tree is empty, new node becomes root
            if (root == NULL)
                return newNode(i, id);

            // Get low value of interval at root
            COORD l = root->i->low;

            // If root's low value is smaller, then new interval goes to
            // left subtree
            if (lessThan(i.low, l))
                root->left = insert(root->left, i, id);

            // Else, new node goes to right subtree.
            else
                root->right = insert(root->right, i, id);

            // Update the max value of this ancestor if needed
            if (lessThan(root->max, i.high))
                root->max = i.high;

            return root;
        }

        // A utility function to check if given two intervals overlap
        bool doOverlap(Interval i1, Interval i2) const
        {
            if (lessThanOrEqual(i1.low, i2.low) && lessThan(i2.high, i1.high))
                return true;
            return false;
        }

        // The main function that searches a given interval i in a given
        // Interval Tree.
        ITNode* overlapSearch(ITNode* root, Interval i) const
        {
            // Base Case, tree is empty
            if (root == NULL)
                return NULL;

            // If given interval overlaps with root
            if (doOverlap(*(root->i), i))
                return root;

            // If left child of root is present and max of left child is
            // greater than or equal to given interval, then i may
            // overlap with an interval is left subtree
            if (root->left != NULL && greaterThanOrEqual(root->left->max, i.high))
                return overlapSearch(root->left, i);

            // Else interval can only overlap with right subtree
            return overlapSearch(root->right, i);
        }

    private:
        bool lessThan(const COORD x, const COORD y) const
        {
            if (x.Y < y.Y)
            {
                return true;
            }
            else if (x.Y == y.Y)
            {
                return x.X < y.X;
            }
            return false;
        }

        bool lessThanOrEqual(const COORD x, const COORD y) const
        {
            if (x.Y < y.Y)
            {
                return true;
            }
            else if (x.Y == y.Y)
            {
                return x.X <= y.X;
            }
            return false;
        }

        bool greaterThan(const COORD x, const COORD y) const
        {
            if (x.Y > y.Y)
            {
                return true;
            }
            else if (x.Y == y.Y)
            {
                return x.X > y.X;
            }
            return false;
        }

        bool greaterThanOrEqual(const COORD x, const COORD y) const
        {
            if (x.Y > y.Y)
            {
                return true;
            }
            else if (x.Y == y.Y)
            {
                return x.X >= y.X;
            }
            return false;
        }
    };
}
