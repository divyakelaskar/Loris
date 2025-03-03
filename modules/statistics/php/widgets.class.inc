<?php declare(strict_types=1);

namespace LORIS\statistics;

use \Psr\Http\Message\ServerRequestInterface;
use \Psr\Http\Message\ResponseInterface;
use \LORIS\Middleware\ETagCalculator;

/**
 * The Widgets Class.
 *
 * This class provides the php code to retrieve the statistics
 * for recruitment and study progression data.
 *
 * @category Loris
 * @package  Statistics
 * @author   Alizée Wickenheiser <alizee.wickenheiser@mcin.ca>
 * @license  http://www.gnu.org/licenses/gpl-3.0.txt GPLv3
 * @link     https://www.github.com/aces/Loris/
 */
class Widgets extends \NDB_Page implements ETagCalculator
{
    /**
     * A cache of the endpoint results, so that it doesn't need to be
     * recalculated for the ETag and handler.
     */
    private $_cache;

    /**
     * The handle function will return a json response.
     *
     * @param ServerRequestInterface $request The incoming PSR7 request
     *
     * @return ResponseInterface The outgoing PSR7 response
     */
    public function handle(ServerRequestInterface $request) : ResponseInterface
    {
        switch ($request->getMethod()) {
        case 'GET':
            $this->_cache = $this->_handleGET($request);
            break;
        default:
            return new \LORIS\Http\Response\JSON\MethodNotAllowed(
                $this->allowedMethods()
            );
        }
        return $this->_cache;
    }

    /**
     * Handles the retrieval of statistical data for the request.
     *
     * @param ServerRequestInterface $request The incoming PSR7 request.
     *
     * @return ResponseInterface The outgoing PSR7 response
     */
    private function _handleGET(ServerRequestInterface $request)
    {
        if (isset($this->_cache)) {
            return $this->_cache;
        }
        $factory = \NDB_Factory::singleton();
        $config  = $factory->config();
        $db      = $request->getAttribute('loris')->getDatabaseConnection();

        $recruitmentTarget = $config->getSetting('recruitmentTarget');

        $totalScans  = $this->_getTotalRecruitment($db);
        $recruitment = [
            'overall' => $this->_createProjectProgressBar(
                'overall',
                "Overall Recruitment",
                $recruitmentTarget,
                $totalScans,
                $db,
            )
        ];

        $projects = \Utility::getProjectList();
        foreach (array_keys($projects) as $projectID) {
            $projectInfo = $config->getProjectSettings($projectID);
            if (is_null($projectInfo)) {
                throw new \LorisException(
                    'No project settings exist in the Database for ' .
                    'project ID ' . intval($projectID)
                );
            }
            $recruitment[$projectID] = $this->_createProjectProgressBar(
                $projectID,
                $projectInfo['Name'],
                $projectInfo['recruitmentTarget'],
                $this->getTotalRecruitmentByProject($db, $projectID),
                $db
            );
        }

        $values = [];
        // Used for the react widget recruitment.js
        $values['recruitment'] = $recruitment;
        // Used for the react widget studyprogression.js
        $values['studyprogression'] = [
            'total_scans' => $totalScans,
            'recruitment' => $recruitment
        ];

        $this->_cache = new \LORIS\Http\Response\JsonResponse($values);

        return $this->_cache;
    }

    /**
     * Gets the total count of candidates associated with a specific project
     *
     * @param \Database $DB The database connection to get the count from.
     *
     * @return int
     */
    private function _getTotalRecruitment(\Database $DB): int
    {
        return $DB->pselectOneInt(
            "SELECT COUNT(*) FROM candidate c
             WHERE c.Active='Y' AND c.Entity_type='Human'
             AND c.RegistrationCenterID <> 1",
            []
        ) ?? 0;
    }

    /**
     * Generates the template data for a progress bar.
     *
     * @param string    $ID                The name of the progress bar being
     *                                     created.
     * @param string    $title             The title to add to the template
     *                                     variables.
     * @param int       $recruitmentTarget The target for this recruitment type.
     * @param int       $totalRecruitment  The total number of candidates of all
     *                                     types.
     * @param \Database $db                The database connection to get data from.
     *
     * @return array Smarty template data
     */
    private function _createProjectProgressBar(
        $ID,
        $title,
        $recruitmentTarget,
        $totalRecruitment,
        \Database $db
    ): array {
        $rv = [
            'total_recruitment' => $totalRecruitment,
            'title'             => $title,
        ];
        if (empty($recruitmentTarget)) {
            return $rv;
        }

        $rv['recruitment_target'] = $recruitmentTarget;
        if ($ID == 'overall') {
            $totalFemales = $this->_getTotalSex($db, "Female");
        } else {
            $totalFemales = $this->getTotalSexByProject($db, "Female", intval($ID));
        }
        $rv['female_total']   = $totalFemales;
        $rv['female_percent']
            = round($totalFemales / $recruitmentTarget * 100);
        if ($ID == 'overall') {
            $totalMales = $this->_getTotalSex($db, "Male");
        } else {
            $totalMales = $this->getTotalSexByProject($db, "Male", intval($ID));
        }
        $rv['male_total']   = $totalMales;
        $rv['male_percent']
            = round($totalMales / $recruitmentTarget * 100);
        if ($totalRecruitment > $recruitmentTarget) {
            $rv['surpassed_recruitment'] = "true";

            $rv['female_full_percent']
                = round($totalFemales / $totalRecruitment * 100);

            $rv['male_full_percent']
                = round($totalMales / $totalRecruitment * 100);
        }
        return $rv;
    }

    /**
     * Gets the total count of candidates of a specific sex
     *
     * @param \Database $db  A database connection to retrieve information
     *                       from.
     * @param string    $sex Biological sex (male or female)
     *
     * @return int
     */
    private function _getTotalSex(\Database $db, string $sex) : int
    {
        return $db->pselectOneInt(
            "SELECT COUNT(c.CandID)
            FROM candidate c
            WHERE c.Sex=:sex AND c.Active='Y' AND c.Entity_type='Human'
            AND c.RegistrationCenterID <> 1",
            ['sex' => $sex]
        ) ?? 0;
    }

    /**
     * Gets the total count of candidates of a specific sex,
     * associated with a specific project
     *
     * @param \Database $DB        A database connection to retrieve information
     *                             from.
     * @param string    $sex       A biological sex (male or female)
     * @param int       $projectID Project ID
     *
     * @return int
     */
    function getTotalSexByProject(\Database $DB, string $sex, int $projectID) : int
    {
        return $DB->pselectOneInt(
            "SELECT COUNT(c.CandID)
            FROM candidate c
            WHERE c.Sex=:sex AND c.Active='Y' AND c.RegistrationProjectID=:PID
            AND c.Entity_type='Human' AND c.RegistrationCenterID <> 1",
            [
                'sex' => $sex,
                'PID' => "$projectID",
            ]
        ) ?? 0;
    }

    /**
     * Gets the total count of candidates associated with a specific project.
     *
     * @param \Database $db        A database connection to retrieve information
     *                             from.
     * @param int       $projectID The Project ID to get recruitment for.
     *
     * @return int
     */
    function getTotalRecruitmentByProject(\Database $db, int $projectID): int
    {
        return $db->pselectOneInt(
            "SELECT COUNT(*)
             FROM candidate c
             WHERE c.Active='Y'
              AND c.RegistrationProjectID=:PID
              AND c.Entity_type='Human'
              AND c.RegistrationCenterID <> 1",
            ['PID' => "$projectID"]
        ) ?? 0;
    }

    /**
     * The ETagCalculator provides the ability to calculate
     * an ETag for an incoming HTTP request.
     *
     * @param ServerRequestInterface $request The incoming PSR7 request.
     *
     * @return string The value to use for the ETag header.
     */
    public function ETag(ServerRequestInterface $request): string
    {
        return md5(json_encode($this->_handleGET($request)->getBody()));
    }

    /**
     * Return an array of valid HTTP methods for this endpoint.
     *
     * @return string[] Valid versions
     */
    protected function allowedMethods(): array
    {
        return ['GET'];
    }
}
