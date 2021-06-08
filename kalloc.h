struct PageCounts
{
    uint num_init_free_pages;
    uint num_curr_free_pages;
};

extern struct PageCounts free_page_counts;