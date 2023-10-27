/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   redirect.c                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: yutoendo <yutoendo@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/10/24 16:00:51 by yutoendo          #+#    #+#             */
/*   Updated: 2023/10/27 12:29:46 by yutoendo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../include/minishell.h"

bool readline_interrupted = false;

int stash_fd(int fd)
{
    int stash_fd;

    stash_fd = fcntl(fd, F_DUPFD, 10);
    if (stash_fd < 0)
        fatal_error("fcntl");
    if (close(fd) < 0)
        fatal_error("close");
    return (stash_fd);
}

/*
   Here Documents
	   This type of redirection instructs the shell to read input from the
	   current source until a line containing only word (with no trailing
	   blanks) is seen.  All of the lines read up to that point are then used
	   as the standard input for a command.
	   The format of here-documents is:
              <<[-]word
                      here-document
              delimiter
	   No parameter expansion, command substitution, arithmetic expansion, or
	   pathname expansion is performed on word.  If any characters in word are
	   quoted, the delimiter is the result of quote removal on word, and the
	   lines in the here-document are not expanded.  If word is unquoted, all
	   lines of the here-document are subjected to parameter expansion, command
	   substitution, and arithmetic expansion.  In the latter case, the
	   character sequence \<newline> is ignored, and \ must be used to quote
	   the characters \, $, and `.
	   If the redirection operator is <<-, then all leading tab characters are
	   stripped from input lines and the line containing delimiter.  This
	   allows here-documents within shell scripts to be indented in a natural
	   fashion.
*/

int read_heredoc(const char *delimiter, bool is_delim_unquoted)
{
    char *line;
    int pfd[2];

    if (pipe(pfd) < 0)
    {
        fatal_error("pipe");            
    }
    readline_interrupted = false;
    while(1)
    {
        line = readline("> ");
        if (line == NULL)
            break;
        if (readline_interrupted == true)
        {
            free(line);
            break;
        }
        if (strcmp(line, delimiter) == 0)
        {
            free(line);
            break;
        }
        if (is_delim_unquoted == true)
        {
            line = expand_heredoc_line(line);
        }
        dprintf(pfd[1], "%s\n", line);
        free(line);
    }
    close(pfd[1]);
    if (readline_interrupted == true)
    {
        close(pfd[0]);
        return (-1);
    }
    return (pfd[0]);
}

int open_redir_file(t_node *node)
{
    if (node == NULL)
        return (0);
    if (node->kind == ND_PIPELINE)
    {
        if (open_redir_file(node->command) < 0)
            return (-1);
        if (open_redir_file(node->next) < 0)
            return (-1);
        return (0);
    }
    else if (node->kind == ND_SIMPLE_CMD)
    {
        return (open_redir_file(node->redirects));
    }
    else if (node->kind == ND_REDIR_OUT)
    {
        node->file_fd = open(node->filename->word, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    }
    else if (node->kind == ND_REDIR_IN)
    {
        node->file_fd = open(node->filename->word, O_RDONLY);
    }
    else if (node->kind == ND_REDIR_APPEND)
    {
        node->file_fd = open(node->filename->word, O_CREAT | O_WRONLY | O_APPEND, 0644);
    }
    else if (node->kind == ND_REDIR_HEREDOC)
    {
        node->file_fd = read_heredoc(node->delimiter->word, node->is_delim_unquoted);
    }
    else
    {
        assert_error("open_redir_file");
    }
    if (node->file_fd < 0)
    {
        if (node->kind == ND_REDIR_OUT || node->kind == ND_REDIR_APPEND || node->kind == ND_REDIR_IN)
        {
            xperror(node->filename->word);
        }
        return (-1);
    }
    node->file_fd = stash_fd(node->file_fd);
    return (open_redir_file(node->next));
}

bool is_redirect(t_node *node)
{
    if (node->kind == ND_REDIR_OUT)
        return (true);
    else if (node->kind == ND_REDIR_IN)
        return (true);
    else if (node->kind == ND_REDIR_APPEND)
        return (true);
    else if (node->kind == ND_REDIR_HEREDOC)
        return (true);
    return (false);
}

void do_redirect(t_node *redir)
{
    if (redir == NULL)
        return ;
    if (is_redirect(redir) == true)
    {
        redir->stashed_target_fd = stash_fd(redir->target_fd);
        dup2(redir->file_fd, redir->target_fd);
    }
    else
    {
        assert_error("do_redirect");
    }
    do_redirect(redir->next);
}

// Reset must be done from tail to head
void reset_redirect(t_node *redir)
{
    if (redir == NULL)
        return ;
    reset_redirect(redir->next);
    if (is_redirect(redir) == true)
    {
        close(redir->file_fd);
        close(redir->target_fd);
        dup2(redir->stashed_target_fd, redir->target_fd);
    }
    else
    {
        assert_error("reset_redirect");
    }
}